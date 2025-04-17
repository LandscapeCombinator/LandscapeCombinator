// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMToPNG : public HMFetcher
{
public:
	HMToPNG(FString Name0, bool bScaleAltitude0) : Name(Name0), bScaleAltitude(bScaleAltitude0) {};	
	
	FString GetOutputDir() override
	{
		return FPaths::Combine(ImageDownloaderDir, Name + "-PNG");
	}

	void OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

protected:
	FString Name;
	bool bScaleAltitude;
};

#undef LOCTEXT_NAMESPACE
