// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Coordinates/GlobalCoordinates.h"
#include "ImageDownloader/ImageDownloader.h"

#include "ConsoleHelpers/ExternalTool.h"

#include "GenericPlatform/GenericPlatformMisc.h"
#include "CoreMinimal.h"
#include "Landscape.h"
#include "Engine/DecalActor.h"

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
class LANDSCAPECOMBINATOR_API ALandscapeSpawner : public AActor
{
	GENERATED_BODY()

public:
	ALandscapeSpawner();

	/********************
	 * General Settings *
	 ********************/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Meta",
		meta = (DisplayPriority = "-1")
	)
	bool bSilentMode = false;

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
	/* If you are using World Partition, check this option if you want to create landscape streaming proxies. */
	/* This is useful if you have a large landscape, but it might slow things down for small landscapes. */
	bool bCreateLandscapeStreamingProxies = false;
	

	/**********
	 * Decals *
	 **********/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (DisplayPriority = "-1")
	)
	EDecalCreation DecalCreation = EDecalCreation::None;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (EditCondition = "DecalCreation != EDecalCreation::None", EditConditionHides, DisplayPriority = "0")
	)
	int DecalSortOrder = 0;

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

	
	/***********
	 * Actions *
	 ***********/
	
	/* Spawn the Landscape. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "0")
	)
	void SpawnLandscape();
	
	/* Delete the previously spawned landscape. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "1")
	)
	void DeleteLandscape();

	void SpawnLandscape(TFunction<void(ALandscape*)> OnComplete);

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

	void PostEditChangeProperty(struct FPropertyChangedEvent&);
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, DuplicateTransient, Category = "LandscapeSpawner",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TWeakObjectPtr<ALandscape> SpawnedLandscape = nullptr;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, DuplicateTransient, Category = "LandscapeSpawner",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TWeakObjectPtr<ADecalActor> DecalActor = nullptr;

protected:

	UFUNCTION()
	bool IsWMS()
	{
		return HeightmapDownloader && HeightmapDownloader->IsWMS();
	}

	UFUNCTION()
	bool IsMapbox()
	{
		return HeightmapDownloader && HeightmapDownloader->IsMapbox();
	}

	UFUNCTION()
	static bool HasMapboxToken();

	UFUNCTION()
	static bool HasMapTilerToken();
};

#undef LOCTEXT_NAMESPACE