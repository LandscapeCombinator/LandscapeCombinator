// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeMeshSpawner.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "LandscapeCombinator/LandscapeController.h"

#include "ImageDownloader/TilesCounter.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "Coordinates/LevelCoordinates.h"
#include "ImageDownloader/HMDebugFetcher.h"
#include "ImageDownloader/Transformers/HMMerge.h"
#include "LCCommon/LCSettings.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"

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
		LCReporter::ShowError(LOCTEXT("ALandscapeMeshSpawner::DuplicateActor", "Failed to duplicate actor."));
		return nullptr;
	}
}

#endif

bool ALandscapeMeshSpawner::OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated)
{
	Modify();

	if (bDeleteExistingMeshesBeforeSpawningMeshes)
	{
		if (!Execute_Cleanup(this, !bIsUserInitiated)) return false;
	}

	if (!IsValid(HeightmapDownloader))
	{
		LCReporter::ShowError(
			LOCTEXT("ALandscapeMeshSpawner::OnGenerate", "HeightmapDownloader is not set, you may want to create one, or create a new LandscapeMeshSpawner")
		);

		return false;
	}

	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(this->GetWorld(), false);
	if (IsValid(GlobalCoordinates))
	{
		if (bIsUserInitiated && !LCReporter::ShowMessage(
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
			return false;
		}
	}

	FString Name = GetWorld()->GetName() + "-" + LandscapeMeshLabel;
	HMFetcher* Fetcher = HeightmapDownloader->CreateFetcher(bIsUserInitiated, Name, true, false, false, nullptr, GlobalCoordinates);

	if (!Fetcher)
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("NoFetcher", "There was an error while creating the fetcher for Landscape {0}."),
				FText::FromString(LandscapeMeshLabel)
			)
		);

		return false;
	}

	Fetcher->bIsUserInitiated = bIsUserInitiated;

	TArray<FString> Files;
	FString FilesCRS;
	bool bSuccess = Fetcher->Fetch("", TArray<FString>());
	if (bSuccess)
	{
		Files = Fetcher->OutputFiles;
		FilesCRS = Fetcher->OutputCRS;
	}

	delete Fetcher;

	int CmPerPixel = 0;

	if (!IsValid(GlobalCoordinates))
	{
		if (ULCBlueprintLibrary::GetCmPerPixelForCRS(FilesCRS, CmPerPixel))
		{
			bool bThreadSuccess = Concurrency::RunOnGameThreadAndWait([&]() {
				UWorld *World = GetWorld();
				if (!IsValid(World)) return false;
				ALevelCoordinates *LevelCoordinates = World->SpawnActor<ALevelCoordinates>();
				if (!IsValid(LevelCoordinates)) return false;
				GlobalCoordinates = LevelCoordinates->GlobalCoordinates;
				if (!IsValid(GlobalCoordinates)) return false;
		
				FVector4d Coordinates = FVector4d();
				if (!GDALInterface::GetCoordinates(Coordinates, Files)) return false;
		
				GlobalCoordinates->CRS = FilesCRS;
				GlobalCoordinates->CmPerLongUnit = CmPerPixel;
				GlobalCoordinates->CmPerLatUnit = -CmPerPixel;
		
				double MinCoordWidth = Coordinates[0];
				double MaxCoordWidth = Coordinates[1];
				double MinCoordHeight = Coordinates[2];
				double MaxCoordHeight = Coordinates[3];
				GlobalCoordinates->WorldOriginLong = (MinCoordWidth + MaxCoordWidth) / 2;
				GlobalCoordinates->WorldOriginLat = (MinCoordHeight + MaxCoordHeight) / 2;
				return true;
			});

			if (!bThreadSuccess)
			{
				LCReporter::ShowError(
					LOCTEXT("CouldNotCreateLevelCoordinates",
						"There was an error while spawning a Level Coordinates actor."
					)
				);
			}
		}
		else
		{
			LCReporter::ShowError(
				FText::Format(
					LOCTEXT("ErrorBound",
						"Please create a LevelCoordinates actor with CRS: '{0}', and set the scale that you wish here.\n"
						"Then, try again to spawn your landscape."
					),
					FText::FromString(FilesCRS)
				)
			);

			return false;
		}
	}

	if (Files.Num() == 0)
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("NoFiles", "No output files for Landscape {0}."),
				FText::FromString(LandscapeMeshLabel)
			)
		);

		return false;
	}

	ALandscapeMesh *ReusedLandscapeMesh = nullptr;
	for (auto &OutputFile : Files)
	{
		FVector4d ThisFileCoordinates = FVector4d();
		if (!GDALInterface::GetCoordinates(ThisFileCoordinates, OutputFile)) return false;

		if (bReuseExistingMesh)
		{
			if (!IsValid(ReusedLandscapeMesh)) ReusedLandscapeMesh = Cast<ALandscapeMesh>(ExistingLandscapeMesh.GetActor(GetWorld()));

			if (!IsValid(ReusedLandscapeMesh))
			{
				LCReporter::ShowError(
					LOCTEXT(
						"ALandscapeMeshSpawner::OnGenerate::ExistingLandscapeMesh",
						"ExistingLandscapeMesh must point to a valid LandscapeMesh"
					)
				);
				return false;
			}
			if (!ReusedLandscapeMesh->AddHeightmap(HeightmapPriority, ThisFileCoordinates, GlobalCoordinates, OutputFile)) return false;
		}
		else
		{
			ALandscapeMesh *LandscapeMesh = nullptr;
			bool bThreadSuccess = Concurrency::RunOnGameThreadAndWait([this, &LandscapeMesh, ThisFileCoordinates, GlobalCoordinates, OutputFile, SpawnedActorsPathOverride]()
			{
				LandscapeMesh = GetWorld()->SpawnActor<ALandscapeMesh>();
				if (!IsValid(LandscapeMesh)) return false;

				SpawnedLandscapeMeshes.Add(LandscapeMesh);

#if WITH_EDITOR
				if (!LandscapeMeshLabel.IsEmpty())
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
				LCReporter::ShowError(
					LOCTEXT(
						"ALandscapeMeshSpawner::OnGenerate::CouldNotSpawnLandscapeMesh",
						"Could not spawn LandscapeMesh"
					)
				);

				return false;
			}

			if (!bThreadSuccess) return false;
		}
	}

	// here ReusedLandscapeMesh is necessarily non null because it has been set in the (non-empty) for loop
	return Concurrency::RunOnGameThreadAndWait([&](){
		if (bReuseExistingMesh)
		{
			ReusedLandscapeMesh->RegenerateMesh(SplitNormalsAngle);
			ReusedLandscapeMesh->MeshComponent->SetMaterial(0, LandscapeMaterial);
		}

		return true;
	});
}

#undef LOCTEXT_NAMESPACE
