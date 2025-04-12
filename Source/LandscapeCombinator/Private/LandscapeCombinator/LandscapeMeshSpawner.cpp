// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeMeshSpawner.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "LandscapeCombinator/LandscapeController.h"

#include "ImageDownloader/TilesCounter.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "Coordinates/LevelCoordinates.h"
#include "ImageDownloader/HMDebugFetcher.h"
#include "ImageDownloader/Transformers/HMMerge.h"
#include "LCReporter/LCReporter.h"
#include "LCCommon/LCSettings.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "ConcurrencyHelpers/Concurrency.h"

#include "Async/Async.h"
#include "Components/DecalComponent.h"
#include "Internationalization/Regex.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeSubsystem.h"
#include "Misc/MessageDialog.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

ALandscapeMeshSpawner::ALandscapeMeshSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	HeightmapDownloader = CreateDefaultSubobject<UImageDownloader>(TEXT("HeightmapDownloader"));
	PositionBasedGeneration = CreateDefaultSubobject<ULCPositionBasedGeneration >(TEXT("PositionBasedGeneration"));
}

TArray<UObject*> ALandscapeMeshSpawner::GetGeneratedObjects() const
{
	TArray<UObject*> Result;
	for (auto &SpawnedLandscapeMesh : SpawnedLandscapeMeshes)
	{
		if (SpawnedLandscapeMesh.IsValid()) Result.Add(SpawnedLandscapeMesh.Get());
	}
	return Result;
}

void ALandscapeMeshSpawner::DeleteLandscape()
{
	Execute_Cleanup(this, false);
}

#if WITH_EDITOR

AActor *ALandscapeMeshSpawner::Duplicate(FName FromName, FName ToName)
{
	if (ALandscapeMeshSpawner *NewLandscapeSpawner =
		Cast<ALandscapeMeshSpawner>(GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->DuplicateActor(this)))
	{
		NewLandscapeSpawner->SpawnedLandscapeMeshesTag = ULCBlueprintLibrary::ReplaceName(SpawnedLandscapeMeshesTag, FromName, ToName);
		NewLandscapeSpawner->LandscapeMeshLabel = ULCBlueprintLibrary::Replace(LandscapeMeshLabel, FromName.ToString(), ToName.ToString());
		return NewLandscapeSpawner;
	}
	else
	{
		ULCReporter::ShowError(LOCTEXT("ALandscapeMeshSpawner::DuplicateActor", "Failed to duplicate actor."));
		return nullptr;
	}
}

#endif

void ALandscapeMeshSpawner::OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated, TFunction<void(bool)> OnComplete)
{
	Modify();

	if (bDeleteExistingMeshesBeforeSpawningMeshes)
	{
		if (!Execute_Cleanup(this, !bIsUserInitiated))
		{
			if (OnComplete) OnComplete(false);
			return;
		}	
	}

	if (!IsValid(HeightmapDownloader))
	{
		ULCReporter::ShowError(
			LOCTEXT("ALandscapeMeshSpawner::OnGenerate", "HeightmapDownloader is not set, you may want to create one, or create a new LandscapeMeshSpawner")
		);

		if (OnComplete) OnComplete(false);
		return;
	}

	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(this->GetWorld(), false);
	if (IsValid(GlobalCoordinates))
	{
		if (bIsUserInitiated && !ULCReporter::ShowMessage(
			LOCTEXT(
				"ALandscapeMeshSpawner::OnGenerate::ExistingGlobal",
				"There already exists a LevelCoordinates actor. Continue?\n"
				"Press Yes if you want to spawn your landscape with respect to these level coordinates.\n"
				"Press No if you want to stop, then manually delete the existing LevelCoordinates, and then try again."
			),
			"SuppressExistingLevelCoordinates",
			LOCTEXT("ExistingGlobalTitle", "Already Existing Level Coordinates")
		))
		{
			if (OnComplete) OnComplete(false);
			return;
		}
	}

	HMFetcher* Fetcher = HeightmapDownloader->CreateFetcher(LandscapeMeshLabel, true, false, false, nullptr, GlobalCoordinates);

	if (!Fetcher)
	{
		ULCReporter::ShowError(
			FText::Format(
				LOCTEXT("NoFetcher", "There was an error while creating the fetcher for Landscape {0}."),
				FText::FromString(LandscapeMeshLabel)
			)
		);

		if (OnComplete) OnComplete(false);
		return;
	}

	Fetcher->bIsUserInitiated = bIsUserInitiated;

	Fetcher->Fetch("", TArray<FString>(), [Fetcher, OnComplete, SpawnedActorsPathOverride, this](bool bSuccess)
	{
		if (!bSuccess)
		{
			delete Fetcher;
			if (OnComplete) OnComplete(false);
			return;
		}

		int CmPerPixel = 0;
		TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(this->GetWorld(), false);

		if (!GlobalCoordinates && !ULCBlueprintLibrary::GetCmPerPixelForCRS(Fetcher->OutputCRS, CmPerPixel))
		{
			ULCReporter::ShowError(
				FText::Format(
					LOCTEXT("ErrorBound",
						"Please create a LevelCoordinates actor with CRS: '{0}', and set the scale that you wish here.\n"
						"Then, try again to spawn your landscape."
					),
					FText::FromString(Fetcher->OutputCRS)
				)
			);

			delete Fetcher;
			if (OnComplete) OnComplete(false);
			return;
		}

		if (!GlobalCoordinates)
		{
			UWorld *World = GetWorld();
			ALevelCoordinates *LevelCoordinates = World->SpawnActor<ALevelCoordinates>();
			GlobalCoordinates = LevelCoordinates->GlobalCoordinates;

			FVector4d Coordinates = FVector4d();
			if (!GDALInterface::GetCoordinates(Coordinates, Fetcher->OutputFiles))
			{
				delete Fetcher;
				if (OnComplete) OnComplete(false);
				return;
			}

			GlobalCoordinates->CRS = Fetcher->OutputCRS;
			GlobalCoordinates->CmPerLongUnit = CmPerPixel;
			GlobalCoordinates->CmPerLatUnit = -CmPerPixel;

			double MinCoordWidth = Coordinates[0];
			double MaxCoordWidth = Coordinates[1];
			double MinCoordHeight = Coordinates[2];
			double MaxCoordHeight = Coordinates[3];
			GlobalCoordinates->WorldOriginLong = (MinCoordWidth + MaxCoordWidth) / 2;
			GlobalCoordinates->WorldOriginLat = (MinCoordHeight + MaxCoordHeight) / 2;
		}

		if (Fetcher->OutputFiles.Num() == 0)
		{
			ULCReporter::ShowError(
				FText::Format(
					LOCTEXT("NoFiles", "No output files for Landscape {0}."),
					FText::FromString(LandscapeMeshLabel)
				)
			);

			delete Fetcher;
			if (OnComplete) OnComplete(false);
			return;
		}

		ALandscapeMesh *ReusedLandscapeMesh = nullptr;
		for (auto &OutputFile : Fetcher->OutputFiles)
		{
			FVector4d ThisFileCoordinates = FVector4d();
			if (!GDALInterface::GetCoordinates(ThisFileCoordinates, OutputFile))
			{
				delete Fetcher;
				if (OnComplete) OnComplete(false);
				return;
			}

			if (bReuseExistingMesh)
			{
				if (!IsValid(ReusedLandscapeMesh)) ReusedLandscapeMesh = Cast<ALandscapeMesh>(ExistingLandscapeMesh.GetActor(GetWorld()));

				if (!IsValid(ReusedLandscapeMesh))
				{
					ULCReporter::ShowError(
						LOCTEXT(
							"ALandscapeMeshSpawner::OnGenerate::ExistingLandscapeMesh",
							"ExistingLandscapeMesh must point to a valid LandscapeMesh"
						)
					);

					delete Fetcher;
					if (OnComplete) OnComplete(false);
					return;
				}
				if (!ReusedLandscapeMesh->AddHeightmap(HeightmapPriority, ThisFileCoordinates, GlobalCoordinates, OutputFile))
				{
					delete Fetcher;
					if (OnComplete) OnComplete(false);
					return;
				}
			}
			else
			{
				ALandscapeMesh *LandscapeMesh = nullptr;
				bool bThreadSuccess = Concurrency::RunOnGameThreadAndReturn([this, &LandscapeMesh, ThisFileCoordinates, GlobalCoordinates, OutputFile, SpawnedActorsPathOverride]()
				{
					LandscapeMesh = GetWorld()->SpawnActor<ALandscapeMesh>();
					if (!IsValid(LandscapeMesh)) return false;

					SpawnedLandscapeMeshes.Add(LandscapeMesh);

#if WITH_EDITOR
					LandscapeMesh->SetActorLabel(LandscapeMeshLabel);
					ULCBlueprintLibrary::SetFolderPath2(LandscapeMesh, SpawnedActorsPathOverride, SpawnedActorsPath);
#endif

					if (!SpawnedLandscapeMeshesTag.IsNone()) LandscapeMesh->Tags.Add(SpawnedLandscapeMeshesTag);
					if (!LandscapeMesh->AddHeightmap(0, ThisFileCoordinates, GlobalCoordinates, OutputFile)) return false;
					LandscapeMesh->MeshComponent->SetMaterial(0, LandscapeMaterial);
					return LandscapeMesh->RegenerateMesh(SplitNormalsAngle);
				});


				if (!IsValid(LandscapeMesh))
				{
					ULCReporter::ShowError(
						LOCTEXT(
							"ALandscapeMeshSpawner::OnGenerate::CouldNotSpawnLandscapeMesh",
							"Could not spawn LandscapeMesh"
						)
					);

					delete Fetcher;
					if (OnComplete) OnComplete(false);
					return;
				}

				if (!bThreadSuccess)
				{
					delete Fetcher;
					if (OnComplete) OnComplete(false);
					return;
				}
			}
		}

		// here ReusedLandscapeMesh is necessarily non null because it has been set in the (non-empty) for loop
		if (bReuseExistingMesh)
		{
			ReusedLandscapeMesh->RegenerateMesh(SplitNormalsAngle);
			ReusedLandscapeMesh->MeshComponent->SetMaterial(0, LandscapeMaterial);
		}

		delete Fetcher;
		if (OnComplete) OnComplete(true);
		return;
	});

	return;
}

#undef LOCTEXT_NAMESPACE
