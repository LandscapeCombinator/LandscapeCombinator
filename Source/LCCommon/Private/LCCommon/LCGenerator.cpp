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

	AActor *Self = Cast<AActor>(this);
	if (!Self) return false;

	ULCPositionBasedGeneration* PositionBasedGeneration = Cast<ULCPositionBasedGeneration>(Self->GetComponentByClass(ULCPositionBasedGeneration::StaticClass()));
	if (IsValid(PositionBasedGeneration) && PositionBasedGeneration->bEnablePositionBasedGeneration)
	{
		bool bFoundPosition = false;
		FVector Position(0, 0, 0);
		if (!ULCBlueprintLibrary::GetFirstPlayerPosition(Position) &&
			!ULCBlueprintLibrary::GetEditorViewClientPosition(Position))
		{
			LCReporter::ShowError(LOCTEXT("NoPosition", "Could not get the first player position"));
			return false;
		}

		int Zoom = PositionBasedGeneration->ZoomLevel;		
		FVector2D Location2D, Coordinates;
		Location2D.X = Position.X;
		Location2D.Y = Position.Y;

		UGlobalCoordinates *GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(Self->GetWorld(), true);
		if (!IsValid(GlobalCoordinates))
		{
			LCReporter::ShowError(LOCTEXT("NoGlobalCoordinates", "You must add a Level Coordinates actor before using Position Based Generation"));
			return false;
		}

		if (!GlobalCoordinates->GetCRSCoordinatesFromUnrealLocation(Location2D, "EPSG:4326", Coordinates)) return false;

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
			if (!ConfigureForTiles(Zoom, MinX, MaxX, MinY, MaxY)) return false;
			UE_LOG(LogLCCommon, Log,
				TEXT("All tiles from (%d, %d, %d) to (%d, %d, %d) are missing, generating them now"),
				Zoom, MinX, MinY,
				Zoom, MaxX, MaxY
			);

			Execute_OnGenerateBP(Self, SpawnedActorsPath, bIsUserInitiated);
			if (OnGenerate(SpawnedActorsPath, bIsUserInitiated))
			{
				PositionBasedGeneration->GeneratedTiles.Append(MissingTiles);
				GenerationFinished(true);
				return true;
			}
			else
			{
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
				UE_LOG(LogLCCommon, Log, TEXT("Missing tile (%d, %d, %d)"), Tile.Zoom, Tile.X, Tile.Y);
			}

			for (FTile& Tile : MissingTiles)
			{
				UE_LOG(LogLCCommon, Log, TEXT("Generating tile (%d, %d, %d)"), Tile.Zoom, Tile.X, Tile.Y);
				if (!ConfigureForTiles(Tile.Zoom, Tile.X, Tile.X, Tile.Y, Tile.Y)) return false;

				Execute_OnGenerateBP(Self, SpawnedActorsPath, bIsUserInitiated);
				if (OnGenerate(SpawnedActorsPath, bIsUserInitiated))
				{
					PositionBasedGeneration->GeneratedTiles.Add(Tile);
				}
				else
				{
					GenerationFinished(false);
					return false;
				}
			}

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

			GenerationFinished(true);
			return true;
		}
	}
	else
	{
		Execute_OnGenerateBP(Self, SpawnedActorsPath, bIsUserInitiated);
		bool bSuccess = OnGenerate(SpawnedActorsPath, bIsUserInitiated);
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
	AActor* Self = Cast<AActor>(this);
	if (!IsValid(Self)) return false;
	Self->Modify();

	ULCPositionBasedGeneration *PositionBasedGeneration = Self->FindComponentByClass<ULCPositionBasedGeneration>();
	if (IsValid(PositionBasedGeneration))
	{
		PositionBasedGeneration->ClearGeneratedTilesCache();
	}

	TArray<UObject*> GeneratedObjects = GetGeneratedObjects();

	if (GeneratedObjects.Num() == 0) return true;

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
		else if (IsValid(Object))
		{
			Object->MarkAsGarbage();
		}
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
