// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Coordinates/LevelCoordinates.h"
#include "ImageDownloader/ImageDownloader.h"
#include "LCCommon/LCGenerator.h"

#include "CoreMinimal.h"
#include "Landscape.h"
#include "Engine/DecalActor.h"

#include "LandscapeTexturer.generated.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

UCLASS(BlueprintType)
class LANDSCAPECOMBINATOR_API ALandscapeTexturer : public AActor, public ILCGenerator
{
	GENERATED_BODY()

public:
	ALandscapeTexturer();

	/***********
	 * Actions *
	 ***********/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (DisplayPriority = "0")
	)
	/* Folder used to spawn the actors. This setting is unused when generating from a combination or from blueprints. */
	FName SpawnedActorsPath;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (DisplayPriority = "1")
	)
	int DecalsSortOrder = 0;

	virtual TArray<UObject*> GetGeneratedObjects() const override
	{
		TArray<UObject*> Result;
		for (auto &DecalActor : DecalActors)
		{
			if (IsValid(DecalActor)) Result.Add(DecalActor);
		}
		return Result;
	}

	virtual void OnGenerate(FName SpawnedActorsPathOverride, TFunction<void(bool)> OnComplete) override
	{
		CreateDecal(ALevelCoordinates::GetGlobalCoordinates(this->GetWorld(), false), SpawnedActorsPathOverride, OnComplete);
	}

	virtual bool Cleanup_Implementation(bool bSkipPrompt) override { return DeleteGeneratedObjects(bSkipPrompt); }

	void DownloadImages(TFunction<void(bool)> OnComplete);
	
	/* Download an image and create a decal. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeTexturer",
		meta = (DisplayPriority = "10")
	)
	void CreateDecal() { CreateDecal(ALevelCoordinates::GetGlobalCoordinates(this->GetWorld(), false), FName(), nullptr); };
	
	void CreateDecal(TObjectPtr<UGlobalCoordinates> GlobalCoordinates, FName SpawnedActorsPathOverride, TFunction<void(bool)> OnComplete);

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

protected:

	HMFetcher* Fetcher = nullptr;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, DuplicateTransient, Category = "LandscapeTexturer",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TArray<ADecalActor*> DecalActors;
};

#undef LOCTEXT_NAMESPACE