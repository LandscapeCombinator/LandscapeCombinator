// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Coordinates/LevelCoordinates.h"
#include "ImageDownloader/ImageDownloader.h"
#include "LCCommon/LCGenerator.h"
#include "ConcurrencyHelpers/Concurrency.h"

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
	
	virtual bool ConfigureForTiles(int Zoom, int MinX, int MaxX, int MinY, int MaxY) override
	{
		if (IsValid(ImageDownloader)) return ImageDownloader->ConfigureForTiles(Zoom, MinX, MaxX, MinY, MaxY);
		else
		{
			LCReporter::ShowError(LOCTEXT("Error", "ImageDownloader is not set"));
			return false;
		}
	}

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeTexturer",
		meta = (DisplayPriority = "0")
	)
	/* Folder used to spawn the actors. This setting is unused when generating from a combination or from blueprints. */
	FName SpawnedActorsPath;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeTexturer",
		meta = (DisplayPriority = "1")
	)
	int DecalsSortOrder = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "LandscapeTexturer",
		meta = (DisplayPriority = "2")
	)
	bool bDeleteOldDecalsWhenCreatingDecals = false;

	virtual TArray<UObject*> GetGeneratedObjects() const override
	{
		TArray<UObject*> Result;
		for (auto &DecalActor : DecalActors)
		{
			if (DecalActor.IsValid()) Result.Add(DecalActor.Get());
		}
		return Result;
	}

	virtual bool OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated) override
	{
		return CreateDecals(ALevelCoordinates::GetGlobalCoordinates(this->GetWorld(), false), SpawnedActorsPathOverride, bIsUserInitiated);
	}

	virtual bool Cleanup_Implementation(bool bSkipPrompt) override
	{
		Modify();

		if (DeleteGeneratedObjects(bSkipPrompt))
		{
			DecalActors.Empty();
			return true;
		}
		else return false;
	}

	void DownloadImages(TFunction<void(bool)> OnComplete);
	
	/* Download an image and create a decal. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeTexturer",
		meta = (DisplayPriority = "10")
	)
	void CreateDecals() { 
		Concurrency::RunAsync([this]() { Generate(SpawnedActorsPath, true); });
	}
	
	/* Delete all decal actors asoociated with this Landscape Texturer */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeTexturer",
		meta = (DisplayPriority = "10")
	)
	void ClearDecals() { Execute_Cleanup(this, false); }
	
	bool CreateDecals(TObjectPtr<UGlobalCoordinates> GlobalCoordinates, FName SpawnedActorsPathOverride, bool bIsUserInitiated);

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

	/* Click this to set the WMS coordinates from a Cube or any other actor */
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

	UPROPERTY(
		VisibleAnywhere, Category = "LandscapeTexturer",
		meta = (DisplayPriority = "21")
	)
	TObjectPtr<ULCPositionBasedGeneration> PositionBasedGeneration = nullptr;

protected:

	HMFetcher* Fetcher = nullptr;

	UPROPERTY(
		EditAnywhere, DuplicateTransient, Category = "LandscapeTexturer",
		meta = (EditCondition = "false", EditConditionHides, DisplayPriority = "1000")
	)
	TArray<TSoftObjectPtr<ADecalActor>> DecalActors;
};

#undef LOCTEXT_NAMESPACE