// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "LCCommon/LCGenerator.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "LCCommon/LogLCCommon.h"
#include "Coordinates/LevelCoordinates.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "HAL/ThreadManager.h"

#if WITH_EDITOR
#include "EditorActorFolders.h"
#endif

#define LOCTEXT_NAMESPACE "FActorGeneratorModule"

static double DistanceToLonLatRect(double Lon, double Lat, double MinLon, double MaxLon, double MinLat, double MaxLat)
{
	if (FMath::Fmod(FMath::Fmod(Lon - MinLon, 360.0) + 360.0, 360.0) <= MaxLon - MinLon)
	{
		return FMath::Max3(0.0, MinLat - Lat, Lat - MaxLat);
	}

	const double Phi = FMath::DegreesToRadians(Lat);
	double Best = 180.0;
	for (double EdgeLon : { MinLon, MaxLon })
	{
		const double DLon = FMath::DegreesToRadians(EdgeLon - Lon);
		const double Closest = FMath::RadiansToDegrees(FMath::Atan2(FMath::Sin(Phi), FMath::Cos(Phi) * FMath::Cos(DLon)));
		const double EdgeLats[3] = { MinLat, MaxLat, FMath::Clamp(Closest, MinLat, MaxLat) };
		for (double EdgeLat : EdgeLats)
		{
			const double Psi = FMath::DegreesToRadians(EdgeLat);
			const double CosDistance = FMath::Sin(Phi) * FMath::Sin(Psi) + FMath::Cos(Phi) * FMath::Cos(Psi) * FMath::Cos(DLon);
			Best = FMath::Min(Best, FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosDistance, -1.0, 1.0))));
		}
	}
	return Best;
}

bool ILCGenerator::Generate(FName SpawnedActorsPath, bool bIsUserInitiated)
{
	if (IsInGameThread())
	{
		LCReporter::ShowError(LOCTEXT("GenerateGameThread", "ILCGenerator::Generate cannot be called from the game thread. Use ILCGenerator::GenerateFromGameThread instead."));
		return false;
	}

	Self = Cast<AActor>(this);
	if (!Self.IsValid()) return false;

	if (CurrentStatus != EGeneratorStatus::Generating) Concurrency::SetCancelRequested(false);
	CurrentStatus = EGeneratorStatus::Generating;

	// Read everything we need from the component on the game thread, then only use these copies
	TWeakObjectPtr<ULCPositionBasedGeneration> WeakPBG;
	bool bGroupFirstTiles = false;
	int Zoom = 0, TileDist = 0;
	double MinAltitudeCm = 0, MaxAltitudeCm = 0;
	TSet<FTile> AlreadyGenerated;

	if (Concurrency::RunOnGameThreadAndWait([&]() {
		ULCPositionBasedGeneration* PBG = Self.IsValid() ? Self->FindComponentByClass<ULCPositionBasedGeneration>() : nullptr;
		if (!IsValid(PBG) || !PBG->bEnablePositionBasedGeneration) return false;
		WeakPBG = PBG;
		bGroupFirstTiles = PBG->bGroupFirstTiles && PBG->MaxTiles <= 0;
		Zoom = PBG->ZoomLevel;
		TileDist = PBG->GenerateAllTilesAtDistance;
		MaxAltitudeCm = PBG->MaxGenerationAltitudeKm * 100000.0;
		MinAltitudeCm = PBG->MinGenerationAltitudeKm * 100000.0;
		bGeodeticTiles = PBG->bGeodeticTiles;
		PBG->GeneratedTiles.GetKeys(AlreadyGenerated);
		return true;
	}))
	{
		FVector Position(0, 0, 0);
		if (!ULCBlueprintLibrary::GetFirstPlayerPosition(Self->GetWorld(), Position) &&
			!ULCBlueprintLibrary::GetEditorViewClientPosition(Position))
		{
			LCReporter::ShowError(LOCTEXT("NoPosition", "Could not get the first player position"));
			CurrentStatus = FailedStatus();
			return false;
		}

		FVector2D Location2D, Coordinates;
		Location2D.X = Position.X;
		Location2D.Y = Position.Y;

		UGlobalCoordinates *GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinatesFor(Self.Get(), true);
		if (!IsValid(GlobalCoordinates))
		{
			LCReporter::ShowError(LOCTEXT("NoGlobalCoordinates", "You must add a Level Coordinates actor before using Position Based Generation"));
			CurrentStatus = FailedStatus();
			return false;
		}

		double AltCm = Position.Z;
		if (GlobalCoordinates->IsSphere())
		{
			GlobalCoordinates->FromWorld(Position, Coordinates.X, Coordinates.Y, AltCm);
			Coordinates.X = FMath::Fmod(FMath::Fmod(Coordinates.X + 180.0, 360.0) + 360.0, 360.0) - 180.0;
		}
		else if (!GlobalCoordinates->GetCRSCoordinatesFromUnrealLocation(Location2D, "EPSG:4326", Coordinates))
		{
			CurrentStatus = FailedStatus();
			return false;
		}

		if ((MaxAltitudeCm > 0 && AltCm > MaxAltitudeCm) || (MinAltitudeCm > 0 && AltCm < MinAltitudeCm))
		{
			CurrentStatus = EGeneratorStatus::Idle;
			return true;
		}

		double n = 1 << Zoom;
		const int NY = bGeodeticTiles ? FMath::Max(1, (1 << Zoom) / 2) : (1 << Zoom);
		double LatRad = FMath::DegreesToRadians(Coordinates.Y);
		int CurrentX = (Coordinates.X + 180) / 360 * n;
		int CurrentY = bGeodeticTiles ? (90.0 - Coordinates.Y) / 360.0 * n : (1.0 - asinh(FMath::Tan(LatRad)) / UE_PI) / 2.0 * n;

		CurrentX = FMath::Clamp(CurrentX, 0, n - 1);
		CurrentY = FMath::Clamp(CurrentY, 0, NY - 1);
		int MinX = FMath::Clamp(CurrentX - TileDist, 0, n - 1);
		int MaxX = FMath::Clamp(CurrentX + TileDist, 0, n - 1);
		int MinY = FMath::Clamp(CurrentY - TileDist, 0, NY - 1);
		int MaxY = FMath::Clamp(CurrentY + TileDist, 0, NY - 1);

		UE_LOG(LogLCCommon, Log, TEXT("Zoom = %d, CurrentX = %d, CurrentY = %d, Lon = %.3f, Lat = %.3f"), Zoom, CurrentX, CurrentY, Coordinates.X, Coordinates.Y);

		const int NT = 1 << Zoom;
		const bool bWrapX = GlobalCoordinates->IsSphere() && (CurrentX - TileDist < 0 || CurrentX + TileDist > NT - 1);
		TArray<int> XList;
		for (int DX = -TileDist; DX <= TileDist; ++DX)
		{
			XList.AddUnique(bWrapX ? ((CurrentX + DX) % NT + NT) % NT : FMath::Clamp(CurrentX + DX, 0, NT - 1));
		}
		TArray<FTile> MissingTiles;
		TSet<FTile> WantedTiles;
		if (bGeodeticTiles)
		{
			const double TileDegrees = 360.0 / NT;
			const double MaxDistance = TileDist * TileDegrees + 1e-9;
			const int FirstY = MinY, LastY = MaxY;
			MinX = NT; MaxX = -1; MinY = NY; MaxY = -1;
			for (int Y = FirstY; Y <= LastY; ++Y)
			{
				for (int X = 0; X < NT; ++X)
				{
					const double TileMinLon = -180.0 + X * TileDegrees;
					const double TileMaxLat = 90.0 - Y * TileDegrees;
					if (DistanceToLonLatRect(Coordinates.X, Coordinates.Y, TileMinLon, TileMinLon + TileDegrees, TileMaxLat - TileDegrees, TileMaxLat) > MaxDistance) continue;
					MinX = FMath::Min(MinX, X); MaxX = FMath::Max(MaxX, X);
					MinY = FMath::Min(MinY, Y); MaxY = FMath::Max(MaxY, Y);
					FTile Tile(Zoom, X, Y);
					WantedTiles.Add(Tile);
					if (!AlreadyGenerated.Contains(Tile)) MissingTiles.Add(Tile);
				}
			}
		}
		else
		{
			for (int X : XList)
			{
				for (int Y = MinY; Y <= MaxY; ++Y)
				{
					FTile Tile(Zoom, X, Y);
					WantedTiles.Add(Tile);
					if (!AlreadyGenerated.Contains(Tile)) MissingTiles.Add(Tile);
				}
			}
		}

		TArray<FTile> PendingSnapshot = MissingTiles;
		Concurrency::RunOnGameThreadAndWait([WeakPBG, PendingSnapshot, WantedTiles]() {
			if (!WeakPBG.IsValid()) return false;
			WeakPBG->PendingTiles = TSet<FTile>(PendingSnapshot);
			DestroyObjects(WeakPBG->EvictTiles(WantedTiles, PendingSnapshot.Num()));
			return true;
		});

		TWeakObjectPtr<AActor> WeakSelf = Self;
		auto MarkTilesGenerated = [WeakSelf, WeakPBG](const TArray<FTile>& Tiles, const TArray<TSoftObjectPtr<UObject>>& Objects = {}) -> bool
		{
			return Concurrency::RunOnGameThreadAndWait([WeakSelf, WeakPBG, Tiles, Objects]() {
				if (!WeakSelf.IsValid() || !WeakPBG.IsValid()) return false;
				WeakSelf->Modify();
				WeakPBG->Modify();
				for (const FTile& Tile : Tiles)
				{
					FTileRecord& Record = WeakPBG->GeneratedTiles.FindOrAdd(Tile);
					Record.Objects = Objects;
					Record.LastUsedPass = WeakPBG->CurrentPass;
					WeakPBG->PendingTiles.Remove(Tile);
				}
				return true;
			});
		};

		auto GenerateTracked = [this, SpawnedActorsPath, bIsUserInitiated](TArray<TSoftObjectPtr<UObject>>& NewObjects) -> bool
		{
			auto Snapshot = [this]()
			{
				TSet<TSoftObjectPtr<UObject>> Result;
				Concurrency::RunOnGameThreadAndWait([this, &Result]() {
					for (UObject* Object : GetGeneratedObjects())
					{
						if (IsValid(Object)) Result.Add(TSoftObjectPtr<UObject>(Object));
					}
					return true;
				});
				return Result;
			};

			const TSet<TSoftObjectPtr<UObject>> Before = Snapshot();
			const bool bSuccess = OnGenerate(SpawnedActorsPath, bIsUserInitiated);
			for (const TSoftObjectPtr<UObject>& Object : Snapshot())
			{
				if (!Before.Contains(Object)) NewObjects.Add(Object);
			}
			return bSuccess;
		};

		// if all tiles are missing, we regenerate the whole rectangle
		if (bGroupFirstTiles && !bWrapX && MissingTiles.Num() == (MaxX - MinX + 1) * (MaxY - MinY + 1))
		{
			if (!ConfigureForTiles(Zoom, MinX, MaxX, MinY, MaxY))
			{
				CurrentStatus = FailedStatus();
				return false;
			}

			if (!bIsUserInitiated && IsTileOutsideCoverage())
			{
				UE_LOG(LogLCCommon, Log, TEXT("Tiles from (%d, %d, %d) to (%d, %d, %d) are outside of the source coverage, skipping them"), Zoom, MinX, MinY, Zoom, MaxX, MaxY);
				MarkTilesGenerated(MissingTiles);
				CurrentStatus = EGeneratorStatus::Success;
				GenerationFinished(true);
				return true;
			}
			UE_LOG(LogLCCommon, Log,
				TEXT("All tiles from (%d, %d, %d) to (%d, %d, %d) are missing, generating them now"),
				Zoom, MinX, MinY,
				Zoom, MaxX, MaxY
			);

			TArray<TSoftObjectPtr<UObject>> NewObjects;
			if (GenerateTracked(NewObjects))
			{
				if (!MarkTilesGenerated(MissingTiles, NewObjects))
				{
					UE_LOG(LogLCCommon, Warning, TEXT("Generated tiles, but failed to persist GeneratedTiles cache (actor/component no longer valid)."));
				}
				CurrentStatus = EGeneratorStatus::Success;
				GenerationFinished(true);
				return true;
			}
			else
			{
				CurrentStatus = FailedStatus();
				GenerationFinished(false);
				return false;
			}
		}
		else if (MissingTiles.Num() > 0)
		// we generate tile by tile
		{
			UE_LOG(LogLCCommon, Log, TEXT("The following tiles are missing, generating them one by one now:"))
			for (FTile& Tile : MissingTiles)
			{
				UE_LOG(LogLCCommon, Log, TEXT("Missing Tile (%d, %d, %d)"), Tile.Zoom, Tile.X, Tile.Y);
			}

			bool bAnyTileFailed = false;
			for (FTile& Tile : MissingTiles)
			{
				if (Concurrency::IsCancelRequested())
				{
					CurrentStatus = EGeneratorStatus::Idle;
					GenerationFinished(false);
					return false;
				}
				UE_LOG(LogLCCommon, Log, TEXT("Generating Tile (%d, %d, %d)"), Tile.Zoom, Tile.X, Tile.Y);
				if (!ConfigureForTiles(Tile.Zoom, Tile.X, Tile.X, Tile.Y, Tile.Y))
				{
					UE_LOG(LogLCCommon, Error, TEXT("Failed to configure for Tile (%d, %d, %d)"), Tile.Zoom, Tile.X, Tile.Y);
					bAnyTileFailed = true;
					continue;
				}

				if (!bIsUserInitiated && IsTileOutsideCoverage())
				{
					UE_LOG(LogLCCommon, Log, TEXT("Tile (%d, %d, %d) is outside of the source coverage, skipping it"), Tile.Zoom, Tile.X, Tile.Y);
					MarkTilesGenerated({ Tile });
					continue;
				}

				TArray<TSoftObjectPtr<UObject>> NewObjects;
				if (GenerateTracked(NewObjects))
				{
					UE_LOG(LogLCCommon, Log, TEXT("Finished generating Tile (%d, %d, %d)"), Tile.Zoom, Tile.X, Tile.Y);
					if (!MarkTilesGenerated({ Tile }, NewObjects))
					{
						UE_LOG(LogLCCommon, Warning, TEXT("Generated Tile (%d, %d, %d), but failed to persist it to the cache."), Tile.Zoom, Tile.X, Tile.Y);
					}
				}
				else
				{
					UE_LOG(LogLCCommon, Error, TEXT("Failed to generate Tile (%d, %d, %d)"), Tile.Zoom, Tile.X, Tile.Y);
					bAnyTileFailed = true;
				}
			}

			CurrentStatus = bAnyTileFailed ? FailedStatus() : EGeneratorStatus::Success;
			GenerationFinished(!bAnyTileFailed);
			return !bAnyTileFailed;
		}
		else
		{
			UE_LOG(LogLCCommon, Log,
				TEXT("All tiles from (%d, %d, %d) to (%d, %d, %d) have already been generated."),
				Zoom, MinX, MinY,
				Zoom, MaxX, MaxY
			);

			CurrentStatus = EGeneratorStatus::Success;
			GenerationFinished(true);
			return true;
		}
	}
	else
	{
		bool bSuccess = OnGenerate(SpawnedActorsPath, bIsUserInitiated);
		CurrentStatus = bSuccess ? EGeneratorStatus::Success : FailedStatus();
		GenerationFinished(bSuccess);
		return bSuccess;
	}
}

bool ILCGenerator::DeleteGeneratedObjects(bool bSkipPrompt)
{
	return Concurrency::RunOnGameThreadAndWait([this, bSkipPrompt]() {
		return DeleteGeneratedObjects_GameThread(bSkipPrompt);
	});
}

bool ILCGenerator::DeleteGeneratedObjects_GameThread(bool bSkipPrompt)
{
	Self = Cast<AActor>(this);
	if (!Self.IsValid()) return false;
	Self->Modify();

	ULCPositionBasedGeneration *PositionBasedGeneration = Self->FindComponentByClass<ULCPositionBasedGeneration>();
	if (IsValid(PositionBasedGeneration))
	{
		PositionBasedGeneration->ClearGeneratedTilesCache();
	}

	TArray<UObject*> GeneratedObjects = GetGeneratedObjects();

	if (GeneratedObjects.Num() == 0)
	{
		if (CurrentStatus != EGeneratorStatus::Generating) CurrentStatus = EGeneratorStatus::Idle;
		return true;
	}

	UE_LOG(LogLCCommon, Log, TEXT("There are %d object(s) to delete"), GeneratedObjects.Num());
	if (!bSkipPrompt)
	{
		FString ObjectsString;
		for (UObject* Object : GeneratedObjects)
		{
			if (IsValid(Object)) ObjectsString += Object->GetName() + "\n";
			else
			{
				UE_LOG(LogLCCommon, Warning, TEXT("Skipping delete invalid object"));
			}
		}

		if (!ObjectsString.Replace(TEXT("\n"), TEXT("")).Replace(TEXT("  "), TEXT("")).IsEmpty())
		{
			if (!LCReporter::ShowMessage(
				FText::Format(
					LOCTEXT("ILCGenerator::DeleteGeneratedObjects",
						"The following objects will be deleted:\n{0}\nContinue?"),
					FText::FromString(ObjectsString)
				),
				"SuppressConfirmCleanup",
				LOCTEXT("ConfirmCleanup", "Confirm Cleanup")
			))
			{
				return false;
			}
		}
	}

	DestroyObjects(GeneratedObjects);

	if (CurrentStatus != EGeneratorStatus::Generating) CurrentStatus = EGeneratorStatus::Idle;
	return true;
}

void ILCGenerator::DestroyObjects(const TArray<UObject*>& Objects)
{
	for (UObject* Object: Objects)
	{
		if (AActor *Actor = Cast<AActor>(Object))
		{
			UE_LOG(LogLCCommon, Log, TEXT("DestroyObjects: destroying %s (%s)"),
				*Actor->GetActorNameOrLabel(), *Actor->GetClass()->GetName());
	#if WITH_EDITOR
			UWorld *World = Actor->GetWorld();
			FFolder Folder = Actor->GetFolder();
	#endif
			bool bDestroyed = Actor->Destroy();
			UE_LOG(LogLCCommon, Log, TEXT("DestroyObjects: Destroy() returned %s for %s"),
				bDestroyed ? TEXT("true") : TEXT("false"), *Actor->GetActorNameOrLabel());
	#if WITH_EDITOR
			if (World) ULCBlueprintLibrary::DeleteFolder(*World, Folder);
	#endif
		}
		else if (UActorComponent *Component = Cast<UActorComponent>(Object))
		{
			UE_LOG(LogLCCommon, Log, TEXT("DestroyObjects: destroying component %s"), *Component->GetName());
			Component->DestroyComponent();
		}
		else if (IsValid(Object))
		{
			Object->MarkAsGarbage();
		}
		else
		{
			UE_LOG(LogLCCommon, Warning, TEXT("DestroyObjects: skipped a null/invalid entry in GeneratedObjects"));
		}
	}
}

#if WITH_EDITOR

void ILCGenerator::ChangeRootPath(FName FromRootPath, FName ToRootPath)
{
	TArray<UObject*> GeneratedObjects = GetGeneratedObjects();
	for (auto Object : GeneratedObjects)
	{
		if (AActor *Actor = Cast<AActor>(Object))
		{
			FName CurrentPath = Actor->GetFolderPath();
			if (CurrentPath.ToString().StartsWith(FromRootPath.ToString()))
			{
				FName NewPath = FName(CurrentPath.ToString().Replace(*FromRootPath.ToString(), *ToRootPath.ToString()));
				Actor->SetFolderPath(NewPath);
			}
		}
	}
}

#endif

#undef LOCTEXT_NAMESPACE
