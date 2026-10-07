// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

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

bool ILCGenerator::Generate(FName SpawnedActorsPath, bool bIsUserInitiated)
{
	if (IsInGameThread())
	{
		LCReporter::ShowError(LOCTEXT("GenerateGameThread", "ILCGenerator::Generate cannot be called from the game thread. Use ILCGenerator::GenerateFromGameThread instead."));
		return false;
	}

	Self = Cast<AActor>(this);
	if (!Self.IsValid()) return false;

	CurrentStatus = EGeneratorStatus::Generating;
	Concurrency::SetCancelRequested(false);

	// Read everything we need from the component on the game thread, then only use these copies
	TWeakObjectPtr<ULCPositionBasedGeneration> WeakPBG;
	bool bGroupFirstTiles = false;
	int Zoom = 0, TileDist = 0;
	TSet<FTile> AlreadyGenerated;

	if (Concurrency::RunOnGameThreadAndWait([&]() {
		ULCPositionBasedGeneration* PBG = Self.IsValid() ? Self->FindComponentByClass<ULCPositionBasedGeneration>() : nullptr;
		if (!IsValid(PBG) || !PBG->bEnablePositionBasedGeneration) return false;
		WeakPBG = PBG;
		bGroupFirstTiles = PBG->bGroupFirstTiles;
		Zoom = PBG->ZoomLevel;
		TileDist = PBG->GenerateAllTilesAtDistance;
		AlreadyGenerated = PBG->GeneratedTiles;
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

		UGlobalCoordinates *GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(Self->GetWorld(), true);
		if (!IsValid(GlobalCoordinates))
		{
			LCReporter::ShowError(LOCTEXT("NoGlobalCoordinates", "You must add a Level Coordinates actor before using Position Based Generation"));
			CurrentStatus = FailedStatus();
			return false;
		}

		if (!GlobalCoordinates->GetCRSCoordinatesFromUnrealLocation(Location2D, "EPSG:4326", Coordinates))
		{
			CurrentStatus = FailedStatus();
			return false;
		}

		double n = 1 << Zoom;
		double LatRad = FMath::DegreesToRadians(Coordinates.Y);
		int CurrentX = (Coordinates.X + 180) / 360 * n;
		int CurrentY = (1.0 - asinh(FMath::Tan(LatRad)) / UE_PI) / 2.0 * n;

		CurrentX = FMath::Clamp(CurrentX, 0, n - 1);
		CurrentY = FMath::Clamp(CurrentY, 0, n - 1);
		int MinX = FMath::Clamp(CurrentX - TileDist, 0, n - 1);
		int MaxX = FMath::Clamp(CurrentX + TileDist, 0, n - 1);
		int MinY = FMath::Clamp(CurrentY - TileDist, 0, n - 1);
		int MaxY = FMath::Clamp(CurrentY + TileDist, 0, n - 1);

		UE_LOG(LogLCCommon, Log, TEXT("Zoom = %d, CurrentX = %d, CurrentY = %d"), Zoom, CurrentX, CurrentY);

		TArray<FTile> MissingTiles;
		for (int X = MinX; X <= MaxX; ++X)
		{
			for (int Y = MinY; Y <= MaxY; ++Y)
			{
				FTile Tile(Zoom, X, Y);
				if (!AlreadyGenerated.Contains(Tile)) MissingTiles.Add(Tile);
			}
		}

		TArray<FTile> PendingSnapshot = MissingTiles;
		Concurrency::RunOnGameThreadAndWait([WeakPBG, PendingSnapshot]() {
			if (!WeakPBG.IsValid()) return false;
			WeakPBG->PendingTiles = TSet<FTile>(PendingSnapshot);
			return true;
		});

		TWeakObjectPtr<AActor> WeakSelf = Self;
		auto MarkTilesGenerated = [WeakSelf, WeakPBG](const TArray<FTile>& Tiles) -> bool
		{
			return Concurrency::RunOnGameThreadAndWait([WeakSelf, WeakPBG, Tiles]() {
				if (!WeakSelf.IsValid() || !WeakPBG.IsValid()) return false;
				WeakSelf->Modify();
				WeakPBG->Modify();
				WeakPBG->GeneratedTiles.Append(Tiles);
				for (const FTile& Tile : Tiles)
				{
					WeakPBG->PendingTiles.Remove(Tile);
				}
				return true;
			});
		};

		// if all tiles are missing, we regenerate the whole rectangle
		if (bGroupFirstTiles && MissingTiles.Num() == (MaxX - MinX + 1) * (MaxY - MinY + 1))
		{
			if (!ConfigureForTiles(Zoom, MinX, MaxX, MinY, MaxY))
			{
				CurrentStatus = FailedStatus();
				return false;
			}
			UE_LOG(LogLCCommon, Log,
				TEXT("All tiles from (%d, %d, %d) to (%d, %d, %d) are missing, generating them now"),
				Zoom, MinX, MinY,
				Zoom, MaxX, MaxY
			);

			if (OnGenerate(SpawnedActorsPath, bIsUserInitiated))
			{
				if (!MarkTilesGenerated(MissingTiles))
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
					CurrentStatus = FailedStatus();
					return false;
				}

				if (OnGenerate(SpawnedActorsPath, bIsUserInitiated))
				{
					UE_LOG(LogLCCommon, Log, TEXT("Finished generating Tile (%d, %d, %d)"), Tile.Zoom, Tile.X, Tile.Y);
					if (!MarkTilesGenerated({ Tile }))
					{
						UE_LOG(LogLCCommon, Warning, TEXT("Generated Tile (%d, %d, %d), but failed to persist it to the cache."), Tile.Zoom, Tile.X, Tile.Y);
					}
				}
				else
				{
					UE_LOG(LogLCCommon, Error, TEXT("Failed to generate Tile (%d, %d, %d)"), Tile.Zoom, Tile.X, Tile.Y);
					CurrentStatus = FailedStatus();
					GenerationFinished(false);
					return false;
				}
			}

			CurrentStatus = EGeneratorStatus::Success;
			GenerationFinished(true);
			return true;
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
		CurrentStatus = EGeneratorStatus::Idle;
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

	for (UObject* Object: GeneratedObjects)
	{
		if (AActor *Actor = Cast<AActor>(Object))
		{
			UE_LOG(LogLCCommon, Log, TEXT("DeleteGeneratedObjects_GameThread: destroying %s (%s)"),
				*Actor->GetActorNameOrLabel(), *Actor->GetClass()->GetName());
	#if WITH_EDITOR
			UWorld *World = Actor->GetWorld();
			FFolder Folder = Actor->GetFolder();
	#endif
			bool bDestroyed = Actor->Destroy();
			UE_LOG(LogLCCommon, Log, TEXT("DeleteGeneratedObjects_GameThread: Destroy() returned %s for %s"),
				bDestroyed ? TEXT("true") : TEXT("false"), *Actor->GetActorNameOrLabel());
	#if WITH_EDITOR
			if (World) ULCBlueprintLibrary::DeleteFolder(*World, Folder);
	#endif
		}
		else if (UActorComponent *Component = Cast<UActorComponent>(Object))
		{
			UE_LOG(LogLCCommon, Log, TEXT("DeleteGeneratedObjects_GameThread: destroying component %s"), *Component->GetName());
			Component->DestroyComponent();
		}
		else if (IsValid(Object))
		{
			Object->MarkAsGarbage();
		}
		else
		{
			UE_LOG(LogLCCommon, Warning, TEXT("DeleteGeneratedObjects_GameThread: skipped a null/invalid entry in GeneratedObjects"));
		}
	}

	CurrentStatus = EGeneratorStatus::Idle;
	return true;
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
