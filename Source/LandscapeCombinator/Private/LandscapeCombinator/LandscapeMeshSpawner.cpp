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

bool ALandscapeMeshSpawner::Cleanup_Implementation(bool bSkipPrompt)
{
	Concurrency::SetCancelRequested(false);
	Modify();

	if (!DeleteGeneratedObjects(bSkipPrompt)) return false;
	SpawnedLandscapeMeshes.Empty();

	if (!bReuseExistingMesh) return true;

	const FName OwnerName = GetFName();
	TWeakObjectPtr<ALandscapeMesh> MeshToRegenerate;
	FLastMeshSettings Settings;

	Concurrency::RunOnGameThreadAndWait([&]() {
		ALandscapeMesh* SharedMesh = Cast<ALandscapeMesh>(ExistingLandscapeMesh.GetActor(GetWorld(), false));
		if (!IsValid(SharedMesh)) return true;

		if (SharedMesh->RemoveHeightmaps(OwnerName) == 0) { SharedMesh->Clear(); return true; }

		MeshToRegenerate = SharedMesh;
		Settings = SharedMesh->GetLastSettings();
		return true;
	});

	if (MeshToRegenerate.IsValid())
	{
		Concurrency::RunAsync([MeshToRegenerate, Settings]() {
			ALandscapeMesh* Mesh = MeshToRegenerate.Get();
			if (!IsValid(Mesh)) return;
			if (Mesh->RegenerateMesh(Settings.SplitNormalsAngle, Settings.SplitDirection, Settings.ApronWidth, Settings.ApronDepth))
				ALandscapeMesh::RegisterAndCutLowerPriority(Mesh, Settings.SplitNormalsAngle, Settings.SplitDirection, Settings.ApronWidth, Settings.ApronDepth);
		});
	}

	return true;
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
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	Modify();

	if (bDeleteExistingMeshesBeforeSpawningMeshes && !bReuseExistingMesh)
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

	UWorld *World = GetWorld();
	if (!IsValid(World))
	{
		LCReporter::ShowError(
			LOCTEXT("ALandscapeMeshSpawner::OnGenerate", "Invalid World in LandscapeMeshSpawner")
		);

		return false;
	}

	TWeakObjectPtr<ALandscapeMeshSpawner> WeakThis(this);

	TWeakObjectPtr<ALandscapeMesh> ExistingMesh;
	if (bReuseExistingMesh)
	{
		ALandscapeMesh* FoundMesh = nullptr;
		if (!Concurrency::RunOnGameThreadThrottledAndWait([&]() {
			FoundMesh = Cast<ALandscapeMesh>(ExistingLandscapeMesh.GetActor(World, false));
			return IsValid(FoundMesh);
		}))
		{
			LCReporter::ShowError(
				LOCTEXT("NoExistingMesh",
					"Could not find the existing Landscape Mesh to reuse.\n"
					"Please spawn it first, and check the \"Existing Landscape Mesh\" setting."
				)
			);

			return false;
		}
		ExistingMesh = FoundMesh;
	}

	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World, false);
	if (IsValid(GlobalCoordinates))
	{
		if (bIsUserInitiated && !bReuseExistingMesh && !LCReporter::ShowMessage(
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

	FString Name = World->GetName() + "-" + LandscapeMeshLabel;
	HMFetcher* Fetcher = HeightmapDownloader->CreateFetcher(bIsUserInitiated, Name, true, false, false, false, false, nullptr, GlobalCoordinates);

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
	{
		if (Fetcher->Fetch("", TArray<FString>()))
		{
			Files = Fetcher->OutputFiles;
			FilesCRS = Fetcher->OutputCRS;
		}
	}

	delete Fetcher;

	int CmPerPixel = 0;

	if (!IsValid(GlobalCoordinates))
	{
		if (ULCBlueprintLibrary::GetCmPerPixelForCRS(FilesCRS, CmPerPixel))
		{
			bool bThreadSuccess = Concurrency::RunOnGameThreadThrottledAndWait([WeakThis, &GlobalCoordinates, &Files, FilesCRS, CmPerPixel]() {
				if (!WeakThis.IsValid() || !IsValid(WeakThis->GetWorld())) return false;
				UWorld *World = WeakThis->GetWorld();
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

	const FName OwnerName = GetFName();
	bool bAddedToSharedMesh = false;

	for (auto &OutputFile : Files)
	{
		FVector4d ThisFileCoordinates = FVector4d();
		if (!GDALInterface::GetCoordinates(ThisFileCoordinates, OutputFile)) return false;

		if (bReuseExistingMesh)
		{
			ALandscapeMesh* SharedMesh = ExistingMesh.Get();
			if (IsValid(SharedMesh) && SharedMesh->HasHeightmap(OwnerName, ThisFileCoordinates))
			{
				UE_LOG(LogLandscapeCombinator, Log, TEXT("Skipping heightmap '%s': this area was already added by %s"), *OutputFile, *OwnerName.ToString());
				continue;
			}
		}

		ALandscapeMesh *LandscapeMesh = nullptr;
		bool bThreadSuccess = Concurrency::RunOnGameThreadThrottledAndWait([WeakThis, ExistingMesh, OwnerName, &LandscapeMesh, ThisFileCoordinates, GlobalCoordinates, OutputFile, SpawnedActorsPathOverride]()
		{
			if (!WeakThis.IsValid() || !IsValid(WeakThis->GetWorld())) return false;

			if (WeakThis->bReuseExistingMesh)
			{
				LandscapeMesh = ExistingMesh.Get();
				if (!IsValid(LandscapeMesh)) return false;
				return LandscapeMesh->AddHeightmap(WeakThis->HeightmapPriority, ThisFileCoordinates, GlobalCoordinates, OutputFile, OwnerName);
			}

			UWorld *World = WeakThis->GetWorld();
			LandscapeMesh = World->SpawnActor<ALandscapeMesh>();
			if (!IsValid(LandscapeMesh)) return false;

			WeakThis->SpawnedLandscapeMeshes.Add(LandscapeMesh);

#if WITH_EDITOR
			if (!WeakThis->LandscapeMeshLabel.IsEmpty())
				LandscapeMesh->SetActorLabel(WeakThis->LandscapeMeshLabel);
			ULCBlueprintLibrary::SetFolderPath2(LandscapeMesh, SpawnedActorsPathOverride, WeakThis->SpawnedActorsPath);
#endif

			if (!WeakThis->SpawnedLandscapeMeshesTag.IsNone()) LandscapeMesh->Tags.Add(WeakThis->SpawnedLandscapeMeshesTag);
			if (!LandscapeMesh->AddHeightmap(WeakThis->HeightmapPriority, ThisFileCoordinates, GlobalCoordinates, OutputFile)) return false;
			LandscapeMesh->MeshComponent->SetMaterial(0, WeakThis->LandscapeMaterial);
			return true;
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

		if (!bThreadSuccess)
		{
			UE_LOG(LogLandscapeCombinator, Error, TEXT("Failed to add heightmap '%s'"), *OutputFile);
			return false;
		}

		if (bReuseExistingMesh)
		{
			bAddedToSharedMesh = true;
			continue;
		}

		if (!LandscapeMesh->RegenerateMesh(SplitNormalsAngle, GridSplitDirection, ApronWidth, ApronDepth)) { UE_LOG(LogLandscapeCombinator, Error, TEXT("RegenerateMesh failed after adding '%s'"), *OutputFile); return false; }
		ALandscapeMesh::RegisterAndCutLowerPriority(LandscapeMesh, SplitNormalsAngle, GridSplitDirection, ApronWidth, ApronDepth);
	}

	if (bAddedToSharedMesh)
	{
		ALandscapeMesh* SharedMesh = ExistingMesh.Get();
		if (!IsValid(SharedMesh)) return false;

		if (!SharedMesh->RegenerateMesh(SplitNormalsAngle, GridSplitDirection, ApronWidth, ApronDepth))
		{
			UE_LOG(LogLandscapeCombinator, Error, TEXT("RegenerateMesh failed after adding the heightmaps of %s"), *OwnerName.ToString());
			return false;
		}
		ALandscapeMesh::RegisterAndCutLowerPriority(SharedMesh, SplitNormalsAngle, GridSplitDirection, ApronWidth, ApronDepth);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
