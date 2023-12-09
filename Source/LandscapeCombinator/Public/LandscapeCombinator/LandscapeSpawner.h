// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Coordinates/GlobalCoordinates.h"
#include "ImageDownloader/ImageDownloader.h"

#include "ConsoleHelpers/ExternalTool.h"

#include "CoreMinimal.h"
#include "Landscape.h"

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
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|General",
		meta = (DisplayPriority = "0")
	)
	/* Label of the landscape to create. */
	FString LandscapeLabel = "SpawnedLandscape";

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|General",
		meta = (DisplayPriority = "1")
	)
	/* The scale in the Z-axis of your heightmap, ZScale = 1 corresponds to real-world size. */
	double ZScale = 1;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|General",
		meta = (DisplayPriority = "2")
	)
	/* If you are using World Partition, check this option if you want to create landscape streaming proxies. */
	/* This is useful if you have a large landscape, but it might slow things down for small landscapes. */
	bool bCreateLandscapeStreamingProxies = false;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|ComponentCount",
		meta = (DisplayPriority = "3")
	)
	EComponentsMethod ComponentsMethod = EComponentsMethod::AutoWithoutBorder;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|ComponentCount",
		meta = (
			EditCondition = "ComponentsMethod != EComponentsMethod::Auto && ComponentsMethod != EComponentsMethod::AutoWithoutBorder",
			EditConditionHides, DisplayPriority = "4"
		)
	)
	int QuadsPerSubsection;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|ComponentCount",
		meta = (
			EditCondition = "ComponentsMethod != EComponentsMethod::Auto && ComponentsMethod != EComponentsMethod::AutoWithoutBorder",
			EditConditionHides, DisplayPriority = "5"
		)
	)
	int SectionsPerComponent;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner|ComponentCount",
		meta = (
			EditCondition = "ComponentsMethod != EComponentsMethod::Auto && ComponentsMethod != EComponentsMethod::AutoWithoutBorder",
			EditConditionHides, DisplayPriority = "6"
		)
	)
	FIntPoint ComponentCount;
	
	UPROPERTY(
		VisibleAnywhere, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "10")
	)
	TObjectPtr<UImageDownloader> ImageDownloader;

	
	/***********
	 * Actions *
	 ***********/
	
	/* Spawn the Landscape. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "1")
	)
	void SpawnLandscape();

	/* This deletes all the images, included downloaded files. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "2")
	)
	void DeleteAllImages()
	{
		if (ImageDownloader) ImageDownloader->DeleteAllImages();
	}

	/* This preserves downloaded files but deleted all transformed images. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "3")
	)
	void DeleteAllProcessedImages()
	{
		if (ImageDownloader) ImageDownloader->DeleteAllProcessedImages();
	}

	/* Click this to force reloading the WMS Provider from the URL */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (EditCondition = "IsWMS()", EditConditionHides, DisplayPriority = "4")
	)
	void ForceReloadWMS()
	{
		if (ImageDownloader) ImageDownloader->ForceReloadWMS();
	}

	/* Click this to set the min and max coordinates to the largest possible bounds */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (EditCondition = "IsWMS()", EditConditionHides, DisplayPriority = "5")
	)
	void SetLargestPossibleCoordinates()
	{
		if (ImageDownloader) ImageDownloader->SetLargestPossibleCoordinates();
	}

	/* Click this to set the WMS coordinates from a Location Volume or any other actor*/
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (EditCondition = "IsWMS()", EditConditionHides, DisplayPriority = "6")
	)
	void SetCoordinatesFromActor()
	{
		if (ImageDownloader) ImageDownloader->SetCoordinatesFromActor();
	}
	
	UFUNCTION()
	void SetComponentCountFromMethod();

	void PostEditChangeProperty(struct FPropertyChangedEvent&);

private:

	UFUNCTION()
	bool IsWMS()
	{
		return ImageDownloader && ImageDownloader->IsWMS();
	}
};

#undef LOCTEXT_NAMESPACE