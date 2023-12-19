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

	/***********
	 * Actions *
	 ***********/

	void DownloadImages(TFunction<void(bool)> OnComplete);
	
	/* Download an image and create a decal. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeTexturer",
		meta = (DisplayPriority = "10")
	)
	void CreateDecal();

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

	/* Click this to set the WMS coordinates from a Location Volume or any other actor*/
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeTexturer",
		meta = (EditCondition = "IsWMS()", EditConditionHides, DisplayPriority = "15")
	)
	void SetSourceParameters()
	{
		if (ImageDownloader) ImageDownloader->SetSourceParameters();
	}

	/********************
	 * Image Downloader *
	 ********************/
	
	UPROPERTY(
		VisibleAnywhere, Category = "LandscapeTexturer",
		meta = (DisplayPriority = "20")
	)
	TObjectPtr<UImageDownloader> ImageDownloader;

private:
	HMFetcher* Fetcher = nullptr;
};

#undef LOCTEXT_NAMESPACE