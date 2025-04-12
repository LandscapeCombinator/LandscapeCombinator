// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Coordinates/GlobalCoordinates.h"
#include "ImageDownloader/ImageDownloader.h"

#include "ConsoleHelpers/ExternalTool.h"
#include "LCCommon/LCGenerator.h"
#include "LCCommon/ActorSelection.h"
#include "LandscapeMesh.h"
#include "LCReporter/LCReporter.h"

#include "GenericPlatform/GenericPlatformMisc.h"
#include "Components/DynamicMeshComponent.h"
#include "Landscape.h"
#include "Engine/DecalActor.h"

#include "LandscapeMeshSpawner.generated.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

UCLASS(BlueprintType)
class LANDSCAPECOMBINATOR_API ALandscapeMeshSpawner : public AActor, public ILCGenerator
{
	GENERATED_BODY()

public:
	ALandscapeMeshSpawner();

	virtual TArray<UObject*> GetGeneratedObjects() const override;
	
	virtual bool ConfigureForTiles(int Zoom, int MinX, int MaxX, int MinY, int MaxY) override
	{
		if (IsValid(HeightmapDownloader)) return HeightmapDownloader->ConfigureForTiles(Zoom, MinX, MaxX, MinY, MaxY);
		else
		{
			ULCReporter::ShowError(LOCTEXT("Error", "HeightmapDownloader is not set"));
			return false;
		}
	}

	/********************
	 * General Settings *
	 ********************/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeMeshSpawner",
		meta = (DisplayPriority = "-1")
	)
	/* Label of the landscape to create. */
	FString LandscapeMeshLabel = "SpawnedLandscapeMesh";

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeMeshSpawner",
		meta = (DisplayPriority = "1")
	)
	/* Material to apply to the landscape */
	TObjectPtr<UMaterialInterface> LandscapeMaterial = nullptr;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeMeshSpawner",
		meta = (DisplayPriority = "2")
	)
	/* The scale in the Z-axis of your heightmap, ZScale = 1 corresponds to real-world size. */
	double ZScale = 1;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeMeshSpawner",
		meta = (DisplayPriority = "4")
	)
	/* Folder used to spawn the actors. This setting is unused when generating from a combination or from blueprints. */
	FName SpawnedActorsPath;
	
	UPROPERTY(
		VisibleAnywhere, BlueprintReadWrite, Category = "LandscapeMeshSpawner",
		meta = (DisplayPriority = "10", ShowOnlyInnerProperties)
	)
	TObjectPtr<UImageDownloader> HeightmapDownloader = nullptr;

	UPROPERTY(
		VisibleAnywhere, BlueprintReadWrite, Category = "LandscapeMeshSpawner",
		meta = (DisplayPriority = "11", ShowOnlyInnerProperties)
	)
	TObjectPtr<ULCPositionBasedGeneration> PositionBasedGeneration = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeMeshSpawner", meta = (DisplayPriority = "11"))
	double SplitNormalsAngle = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeMeshSpawner", meta = (DisplayPriority = "12"))
	bool bDeleteExistingMeshesBeforeSpawningMeshes = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeMeshSpawner", meta = (DisplayPriority = "13"))
	bool bReuseExistingMesh = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeMeshSpawner",
		meta = (EditCondition = "bReuseExistingMesh", EditConditionHides, DisplayPriority = "14")
	)
	int HeightmapPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeMeshSpawner",
		meta = (EditCondition = "bReuseExistingMesh", EditConditionHides, DisplayPriority = "14")
	)
	FActorSelection ExistingLandscapeMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LandscapeMeshSpawner",
		meta = (EditCondition = "!bReuseExistingMesh", EditConditionHides, DisplayPriority = "20")
	)
	/* Tag to add on the spawned landscape meshes. */
	FName SpawnedLandscapeMeshesTag;

	
	/***********
	 * Actions *
	 ***********/

	/* Spawn the Landscape. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeMeshSpawner",
		meta = (DisplayPriority = "-1")
	)
	void SpawnLandscape() { Generate(SpawnedActorsPath, true, nullptr); };

	virtual void OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated, TFunction<void(bool)> OnComplete) override;

	virtual bool Cleanup_Implementation(bool bSkipPrompt) override
	{
		Modify();

		if (DeleteGeneratedObjects(bSkipPrompt))
		{
			SpawnedLandscapeMeshes.Empty();
			return true;
		}
		else
		{
			return false;
		}
	}

#if WITH_EDITOR
	virtual AActor* Duplicate(FName FromName, FName ToName) override;
#endif

	/* Delete the previously spawned landscape */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeMeshSpawner",
		meta = (DisplayPriority = "0")
	)
	void DeleteLandscape();

protected:

	UPROPERTY(
		EditAnywhere, DuplicateTransient, Category = "LandscapeMeshSpawner",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TArray<TSoftObjectPtr<ALandscapeMesh>> SpawnedLandscapeMeshes;
};

#undef LOCTEXT_NAMESPACE
