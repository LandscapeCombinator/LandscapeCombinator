// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Coordinates/LevelCoordinates.h"
#include "ImageDownloader/ImageDownloader.h"

#include "CoreMinimal.h"
#include "Landscape.h"

#include "LandscapeTexturer.generated.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

UCLASS(BlueprintType)
class LANDSCAPECOMBINATOR_API ALandscapeTexturer : public AActor
{
	GENERATED_BODY()

public:
	ALandscapeTexturer();
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeTexturer|General",
		meta = (DisplayPriority = "0")
	)
	TObjectPtr<ALandscape> TargetLandscape;
	
	UPROPERTY(
		EditAnywhere, Category = "LandscapeTexturer|General",
		meta = (DisplayPriority = "1")
	)
	/* Check this if you want to force resize the images to have the exact same pixel size as your landscape */
	bool bResizeImages;

	/***********
	 * Actions *
	 ***********/
	
	/* Download and prepare PNG images for TargetLandscape on the computer. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeTexturer",
		meta = (DisplayPriority = "10")
	)
	void PrepareImagesForLandscape();
	
	/* Set the ImageDownloader coordinates using the TargetLandscape LandscapeController component. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeTexturer",
		meta = (DisplayPriority = "11")
	)
	void SetCoordinatesFromLandscape();

	/* This preserves downloaded files but deleted all transformed images. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeTexturer",
		meta = (DisplayPriority = "12")
	)
	void DeleteAllProcessedImages()
	{
		if (ImageDownloader) ImageDownloader->DeleteAllProcessedImages();
	}

	/* Click this to force reloading the WMS Provider from the URL */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeTexturer",
		meta = (EditCondition = "IsWMS()", EditConditionHides, DisplayPriority = "13")
	)
	void ForceReloadWMS()
	{
		if (ImageDownloader) ImageDownloader->ForceReloadWMS();
	}

	/* Click this to set the min and max coordinates to the largest possible bounds */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeTexturer",
		meta = (EditCondition = "IsWMS()", EditConditionHides, DisplayPriority = "14")
	)
	void SetLargestPossibleCoordinates()
	{
		if (ImageDownloader) ImageDownloader->SetLargestPossibleCoordinates();
	}

	/********************
	 * Image Downloader *
	 ********************/
	
	UPROPERTY(
		VisibleAnywhere, Category = "LandscapeTexturer",
		meta = (DisplayPriority = "20")
	)
	TObjectPtr<UImageDownloader> ImageDownloader;
};

#undef LOCTEXT_NAMESPACE