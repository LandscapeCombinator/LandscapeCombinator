// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Coordinates/GlobalCoordinates.h"
#include "ImageDownloader/ImageDownloader.h"

#include "ConsoleHelpers/ExternalTool.h"
#include "LCCommon/LCGenerator.h"
#include "LCCommon/ActorSelection.h"
#include "LCReporter/LCReporter.h"

#include "GenericPlatform/GenericPlatformMisc.h"
#include "CoreMinimal.h"
#include "Landscape.h"
#include "Engine/DecalActor.h"
#include "WorldPartition/WorldPartition.h"
#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LandscapeStreamingProxy.h"

#include "LandscapeSpawner.generated.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"


UENUM(BlueprintType)
enum class EComponentsMethod : uint8
{
	/* Enter the Quads Per Section, Section per Components, and Components Counts manually */
	Manual,

	/* When the landscape components do not exactly match with the total size of the heightmaps,
	 * Unreal Engine adds some padding at the border. Check this option if you are willing to
	 * drop some data at the border in order to make your heightmaps match the landscape
	 * components. */
	AutoWithoutBorder,
	
	/* Let Unreal Engine decide the component count based on the heightmap resolution. */
	Auto,

	Preset_8129_8129,
	Preset_4033_4033,
	Preset_2017_2017,
	Preset_1009_1009,
	Preset_1009_1009_2,
	Preset_505_505,
	Preset_505_505_2,
	Preset_253_253,
	Preset_253_253_2,
	Preset_127_127,
	Preset_127_127_2,
};

UENUM(BlueprintType)
enum class EDecalCreation : uint8
{
	None,
	Mapbox,
	MapTiler UMETA(DisplayName = "MapTiler"),
};

UCLASS(BlueprintType)
class LANDSCAPECOMBINATOR_API ALandscapeSpawner : public AActor, public ILCGenerator
{
	GENERATED_BODY()

public:
	ALandscapeSpawner();
	
	virtual bool ConfigureForTiles(int Zoom, int MinX, int MaxX, int MinY, int MaxY) override
	{
		if (IsValid(HeightmapDownloader)) return HeightmapDownloader->ConfigureForTiles(Zoom, MinX, MaxX, MinY, MaxY);
		else
		{
			ULCReporter::ShowError(LOCTEXT("Error", "HeightmapDownloader is not set"));
			return false;
		}
	}

	virtual TArray<UObject*> GetGeneratedObjects() const override;

	/********************
	 * General Settings *
	 ********************/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (DisplayPriority = "-1")
	)
	/* Label of the landscape to create. */
	FString LandscapeLabel = "SpawnedLandscape";

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (DisplayPriority = "0")
	)
	/* Tag to add on the spawned landscape. */
	FName LandscapeTag = "lc";

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (DisplayPriority = "1")
	)
	/* Material to apply to the landscape */
	TObjectPtr<UMaterialInterface> LandscapeMaterial = nullptr;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (DisplayPriority = "2")
	)
	/* The scale in the Z-axis of your heightmap, ZScale = 1 corresponds to real-world size. */
	double ZScale = 1;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (DisplayPriority = "3")
	)
	/**
	 * If you are using World Partition, check this option if you want to create landscape streaming proxies.
	 * This is useful if you have a large landscape, but it might slow things down for small landscapes.
	 * Make sure to adjust the cell size based on the expected size of your landscapes, or you'll get too many
	 * Landscape Streaming Proxies for the engine to handle.
	 */
	bool bCreateLandscapeStreamingProxies = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (DisplayPriority = "4")
	)
	/* Folder used to spawn the actors. This setting is unused when generating from a combination or from blueprints. */
	FName SpawnedActorsPath;
	
	/************
	 * Blending *
	 ************/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Blending",
		meta = (DisplayPriority = "0")
	)
	bool bBlendLandscapeWithAnotherLandscape = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Blending",
		meta = (EditCondition = "bBLendLandscapeWithAnotherLandscape", EditConditionHides, DisplayPriority = "1")
	)
	FActorSelection LandscapeToBlendWith;

	/**********
	 * Decals *
	 **********/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (DisplayPriority = "-10")
	)
	EDecalCreation DecalCreation = EDecalCreation::None;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (EditCondition = "DecalCreation != EDecalCreation::None", EditConditionHides, DisplayPriority = "-5")
	)
	bool bDecalMergeImages = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (EditCondition = "DecalCreation != EDecalCreation::None", EditConditionHides, DisplayPriority = "0")
	)
	int DecalsSortOrder = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (EditCondition = "DecalCreation == EDecalCreation::Mapbox && !HasMapboxToken()", EditConditionHides, DisplayPriority = "1")
	)
	FString Decals_Mapbox_Token = "";

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (EditCondition = "DecalCreation == EDecalCreation::Mapbox", EditConditionHides, DisplayPriority = "2")
	)
	int Decals_Mapbox_Zoom = 9;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (EditCondition = "DecalCreation == EDecalCreation::Mapbox", EditConditionHides, DisplayPriority = "3")
	)
	bool Decals_Mapbox_2x = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (EditCondition = "DecalCreation == EDecalCreation::MapTiler && !HasMapTilerToken()", EditConditionHides, DisplayPriority = "1")
	)
	FString Decals_MapTiler_Token = "";

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (EditCondition = "DecalCreation == EDecalCreation::MapTiler", EditConditionHides, DisplayPriority = "2")
	)
	int Decals_MapTiler_Zoom = 9;
	
	

	/********************
	 * Components Count *
	 ********************/	

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ComponentCount",
		meta = (DisplayPriority = "3")
	)
	EComponentsMethod ComponentsMethod = EComponentsMethod::AutoWithoutBorder;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ComponentCount",
		meta = (
			EditCondition = "ComponentsMethod != EComponentsMethod::Auto && ComponentsMethod != EComponentsMethod::AutoWithoutBorder",
			EditConditionHides, DisplayPriority = "4"
		)
	)
	int QuadsPerSubsection;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ComponentCount",
		meta = (
			EditCondition = "ComponentsMethod != EComponentsMethod::Auto && ComponentsMethod != EComponentsMethod::AutoWithoutBorder",
			EditConditionHides, DisplayPriority = "5"
		)
	)
	int SectionsPerComponent;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ComponentCount",
		meta = (
			EditCondition = "ComponentsMethod != EComponentsMethod::Auto && ComponentsMethod != EComponentsMethod::AutoWithoutBorder",
			EditConditionHides, DisplayPriority = "6"
		)
	)
	FIntPoint ComponentCount;
	
	UPROPERTY(
		VisibleAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "10", ShowOnlyInnerProperties)
	)
	TObjectPtr<UImageDownloader> HeightmapDownloader = nullptr;
	
	UPROPERTY(BlueprintReadWrite, Category = "LandscapeSpawner")
	TObjectPtr<UImageDownloader> DecalDownloader = nullptr;

	UPROPERTY(
		VisibleAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "11", ShowOnlyInnerProperties)
	)
	TObjectPtr<ULCPositionBasedGeneration> PositionBasedGeneration = nullptr;
	
	/***********
	 * Actions *
	 ***********/
	
#if WITH_EDITOR

	/* Spawn the Landscape. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "-1")
	)
	void SpawnLandscape() { SpawnLandscape(SpawnedActorsPath, nullptr); };

	void SpawnLandscape(FName SpawnedActorsPathOverride, TFunction<void(ALandscape*)> OnComplete);
	virtual void OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated, TFunction<void(bool)> OnComplete) override;

	virtual bool Cleanup_Implementation(bool bSkipPrompt) override
	{
		Modify();

		if (DeleteGeneratedObjects(bSkipPrompt))
		{
			SpawnedLandscape = nullptr;
			DecalActors.Empty();
			SpawnedLandscapeStreamingProxies.Empty();
			return true;
		}
		else
		{
			return false;
		}
	}
	virtual AActor* Duplicate(FName FromName, FName ToName) override;

#endif
	
	/* Delete the previously spawned landscape and decal. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "0")
	)
	void DeleteLandscape();

	/* This deletes all the images, included downloaded files. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "2")
	)
	void DeleteAllImages()
	{
		if (HeightmapDownloader) HeightmapDownloader->DeleteAllImages();
	}

	/* This preserves downloaded files but deleted all transformed images. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "3")
	)
	void DeleteAllProcessedImages()
	{
		if (HeightmapDownloader) HeightmapDownloader->DeleteAllProcessedImages();
	}

	/* Click this to force reloading the WMS Provider from the URL */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (EditCondition = "IsWMS()", EditConditionHides, DisplayPriority = "4")
	)
	void ForceReloadWMS()
	{
		if (HeightmapDownloader) HeightmapDownloader->ForceReloadWMS();
	}

	/* Click this to set the min and max coordinates to the largest possible bounds */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (EditCondition = "IsWMS()", EditConditionHides, DisplayPriority = "5")
	)
	void SetLargestPossibleCoordinates()
	{
		if (HeightmapDownloader) HeightmapDownloader->SetLargestPossibleCoordinates();
	}

	/* Click this to set the WMS coordinates from a Location Volume or any other actor*/
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (EditCondition = "IsWMS()", EditConditionHides, DisplayPriority = "6")
	)
	void SetSourceParameters()
	{
		if (HeightmapDownloader) HeightmapDownloader->SetSourceParameters();
	}
	
	UFUNCTION()
	void SetComponentCountFromMethod();

#if WITH_EDITOR

	void PostEditChangeProperty(struct FPropertyChangedEvent&);

 #endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DuplicateTransient, Category = "LandscapeSpawner",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TSoftObjectPtr<ALandscape> SpawnedLandscape = nullptr;

	UPROPERTY(EditAnywhere, DuplicateTransient, Category = "LandscapeSpawner",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TArray<TSoftObjectPtr<ALandscapeStreamingProxy>> SpawnedLandscapeStreamingProxies;
	
	UPROPERTY(EditAnywhere, DuplicateTransient, Category = "LandscapeSpawner",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TArray<TSoftObjectPtr<ADecalActor>> DecalActors;

protected:

	UFUNCTION()
	bool IsWMS()
	{
		return IsValid(HeightmapDownloader) && HeightmapDownloader->IsWMS();
	}

	UFUNCTION()
	bool IsMapbox()
	{
		return IsValid(HeightmapDownloader) && HeightmapDownloader->IsMapbox();
	}

	UFUNCTION()
	static bool HasMapboxToken();

	UFUNCTION()
	static bool HasMapTilerToken();
};

#undef LOCTEXT_NAMESPACE