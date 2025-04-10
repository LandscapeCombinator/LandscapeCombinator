// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LCCommon/LCGenerator.h"
#include "LCReporter/LCReporter.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "LCCommon/LogLCCommon.h"
#include "Coordinates/LevelCoordinates.h"
#include "ConcurrencyHelpers/Concurrency.h"

#if WITH_EDITOR
#include "EditorActorFolders.h"
#endif

#define LOCTEXT_NAMESPACE "FActorGeneratorModule"

void ILCGenerator::Generate(FName SpawnedActorsPath, bool bIsUserInitiated,  TFunction<void(bool)> OnComplete)
{
	AActor *Self = Cast<AActor>(this);
	if (!Self)
	{
		if (OnComplete) OnComplete(false);
		return;
	}

	TFunction<void(bool)> OnCompleteReport = [OnComplete, this](bool bSuccess) {
		GenerationFinished(bSuccess);
		if (OnComplete) OnComplete(bSuccess);
	};

	ULCPositionBasedGeneration* PositionBasedGeneration = Cast<ULCPositionBasedGeneration>(Self->GetComponentByClass(ULCPositionBasedGeneration::StaticClass()));
	if (IsValid(PositionBasedGeneration) && PositionBasedGeneration->bEnablePositionBasedGeneration)
	{
		bool bFoundPosition = false;
		FVector Position(0, 0, 0);
		if (!ULCBlueprintLibrary::GetFirstPlayerPosition(Position) &&
			!ULCBlueprintLibrary::GetEditorViewClientPosition(Position))
		{
			ULCReporter::ShowError(LOCTEXT("NoPosition", "Could not get the first player position"));
			if (OnComplete) OnComplete(false);
			return;
		}

		int Zoom = PositionBasedGeneration->ZoomLevel;		
		FVector2D Location2D, Coordinates;
		Location2D.X = Position.X;
		Location2D.Y = Position.Y;

		UGlobalCoordinates *GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(Self->GetWorld(), true);
		if (!IsValid(GlobalCoordinates))
		{
			ULCReporter::ShowError(LOCTEXT("NoGlobalCoordinates", "You must add a Level Coordinates actor before using Position Based Generation"));
			if (OnComplete) OnComplete(false);
			return;
		}

		if (!GlobalCoordinates->GetCRSCoordinatesFromUnrealLocation(Location2D, "EPSG:4326", Coordinates))
		{
			if (OnComplete) OnComplete(false);
			return;
		}

		double n = 1 << Zoom;
		double LatRad = FMath::DegreesToRadians(Coordinates.Y);
		int CurrentX = (Coordinates.X + 180) / 360 * n;
		int CurrentY = (1.0 - asinh(FMath::Tan(LatRad)) / UE_PI) / 2.0 * n;

		int TileDist = PositionBasedGeneration->GenerateAllTilesAtDistance;
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
				if (!PositionBasedGeneration->GeneratedTiles.Contains(Tile)) MissingTiles.Add(Tile);
			}
		}

		// if all tiles are missing, we regenerate the whole rectangle
		if (MissingTiles.Num() == (MaxX - MinX + 1) * (MaxY - MinY + 1))
		{
			if (!ConfigureForTiles(Zoom, MinX, MaxX, MinY, MaxY))
			{
				if (OnComplete) OnComplete(false);
				return;
			}
			UE_LOG(LogLCCommon, Log,
				TEXT("All tiles from (%d, %d, %d) to (%d, %d, %d) are missing, generating them now"),
				Zoom, MinX, MinY,
				Zoom, MaxX, MaxY
			);

			Execute_OnGenerateBP(Cast<UObject>(this), SpawnedActorsPath, bIsUserInitiated);
			auto OnCompleteSaveTiles = [PositionBasedGeneration, MissingTiles, OnCompleteReport](bool bSuccess)
			{
				if (bSuccess) { PositionBasedGeneration->GeneratedTiles.Append(MissingTiles); }
				if (OnCompleteReport) OnCompleteReport(bSuccess);
			};
			OnGenerate(SpawnedActorsPath, bIsUserInitiated, OnCompleteSaveTiles);
			return;
		}
		else if (MissingTiles.Num() > 0)
		// we generate tile by tile
		{
			UE_LOG(LogLCCommon, Log, TEXT("The following tiles are missing, generating them one by one now:"))
			for (FTile& Tile : MissingTiles)
			{
				UE_LOG(LogLCCommon, Log, TEXT("Missing tile (%d, %d, %d)"), Tile.Zoom, Tile.X, Tile.Y);
			}

			Concurrency::RunSuccessively<FTile>(
				MissingTiles,
				[this, bIsUserInitiated, SpawnedActorsPath, PositionBasedGeneration](FTile Tile, TFunction<void(bool)> OnCompleteOne)
				{
					if (!ConfigureForTiles(Tile.Zoom, Tile.X, Tile.X, Tile.Y, Tile.Y))
					{
						if (OnCompleteOne) OnCompleteOne(false);
						return;
					}
					UE_LOG(LogLCCommon, Log, TEXT("Generating tile (%d, %d, %d)"), Tile.Zoom, Tile.X, Tile.Y);
					Execute_OnGenerateBP(Cast<UObject>(this), SpawnedActorsPath, bIsUserInitiated);
					auto OnCompleteOneSaveTile = [PositionBasedGeneration, OnCompleteOne, Tile](bool bSuccess)
					{
						if (bSuccess) { PositionBasedGeneration->GeneratedTiles.Add(Tile); }
						if (OnCompleteOne) OnCompleteOne(bSuccess);
					};
					OnGenerate(SpawnedActorsPath, bIsUserInitiated, OnCompleteOneSaveTile);
					return;
				},
				OnCompleteReport
			);
		}
		else
		{
			UE_LOG(LogLCCommon, Log,
				TEXT("All tiles from (%d, %d, %d) to (%d, %d, %d) have already been generated."),
				Zoom, MinX, MinY,
				Zoom, MaxX, MaxY
			);

			if (OnCompleteReport) OnCompleteReport(true);
		}
	}
	else
	{
		Execute_OnGenerateBP(Cast<UObject>(this), SpawnedActorsPath, bIsUserInitiated);
		OnGenerate(SpawnedActorsPath, bIsUserInitiated, OnCompleteReport);
	}
}

bool ILCGenerator::DeleteGeneratedObjects(bool bSkipPrompt)
{
	AActor* Self = Cast<AActor>(this);
	if (!IsValid(Self)) return false;

	ULCPositionBasedGeneration *PositionBasedGeneration = Self->FindComponentByClass<ULCPositionBasedGeneration>();
	if (IsValid(PositionBasedGeneration))
	{
		PositionBasedGeneration->ClearGeneratedTilesCache();
	}

	TArray<UObject*> GeneratedObjects = GetGeneratedObjects();
	if (GeneratedObjects.Num() == 0) return true;

	if (!bSkipPrompt)
	{
		FString ObjectsString;
		for (UObject* Object : GeneratedObjects)
			if (IsValid(Object)) ObjectsString += Object->GetName() + "\n";

		if (!ULCReporter::ShowMessage(
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

	for (UObject* Object: GeneratedObjects)
	{
		if (AActor *Actor = Cast<AActor>(Object))
		{
#if WITH_EDITOR
			UWorld *World = Actor->GetWorld();
			FFolder Folder = Actor->GetFolder();
#endif

			Actor->Destroy();

#if WITH_EDITOR
			if (World) ULCBlueprintLibrary::DeleteFolder(*World, Folder);
#endif
		}
		else if (UActorComponent *Component = Cast<UActorComponent>(Object))
		{
			Component->DestroyComponent();
		}
		else if (IsValid(Object)) Object->MarkAsGarbage();
	}
	GeneratedObjects.Empty();
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
