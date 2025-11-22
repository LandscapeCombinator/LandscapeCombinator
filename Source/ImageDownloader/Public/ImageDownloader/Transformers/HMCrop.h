// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"
#include "ImageDownloader/ParametersSelection.h"

class IMAGEDOWNLOADER_API HMCrop : public HMFetcher
{
public:
	HMCrop(FString Name0, bool bCropFollowingParametersSelection0, FParametersSelection ParametersSelection0, AActor *CroppingActor0) :
		Name(Name0),
		bCropFollowingParametersSelection(bCropFollowingParametersSelection0),
		ParametersSelection(ParametersSelection0),
		CroppingActor(CroppingActor0)
	{
	};

	FString GetOutputDir() override
	{
		return FPaths::Combine(ImageDownloaderDir, Name + "-Crop");
	}
	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

protected:
	FString Name;
	bool bCropFollowingParametersSelection;
	FParametersSelection ParametersSelection;
	AActor *CroppingActor;
};
