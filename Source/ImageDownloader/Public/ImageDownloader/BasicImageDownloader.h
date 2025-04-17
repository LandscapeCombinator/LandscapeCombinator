// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Coordinates/LevelCoordinates.h"
#include "ImageDownloader/ImageDownloader.h"

#include "CoreMinimal.h"
#include "Landscape.h"

#include "BasicImageDownloader.generated.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

UCLASS(BlueprintType)
class IMAGEDOWNLOADER_API ABasicImageDownloader : public AActor
{
	GENERATED_BODY()

public:
	ABasicImageDownloader();

	/***********
	 * Actions *
	 ***********/
	
	/* Download and prepare PNG images for TargetLandscape on the computer. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "BasicImageDownloader",
		meta = (DisplayPriority = "10")
	)
	void DownloadImages();

	/* Click this to force reloading the WMS Provider from the URL */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "BasicImageDownloader",
		meta = (EditCondition = "IsWMS()", EditConditionHides, DisplayPriority = "13")
	)
	void ForceReloadWMS()
	{
		if (ImageDownloader) ImageDownloader->ForceReloadWMS();
	}

	/* Click this to set the min and max coordinates to the largest possible bounds */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "BasicImageDownloader",
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
		EditAnywhere, BlueprintReadWrite, Category = "BasicImageDownloader",
		meta = (DisplayPriority = "10")
	)
	FString Name = "ImagesName";
	
	UPROPERTY(
		VisibleAnywhere, Category = "BasicImageDownloader",
		meta = (DisplayPriority = "20")
	)
	TObjectPtr<UImageDownloader> ImageDownloader;
};

#undef LOCTEXT_NAMESPACE