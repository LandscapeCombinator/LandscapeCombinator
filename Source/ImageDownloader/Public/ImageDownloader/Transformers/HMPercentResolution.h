// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMPercentResolution : public HMFetcher
{
public:
	HMPercentResolution(FString Name0, int PrecisionPercent0) : Name(Name0), PrecisionPercent(PrecisionPercent0) {};
	
	FString GetOutputDir() override
	{
		return FPaths::Combine(ImageDownloaderDir, Name + "-PercentResolution" + FString::FromInt(PrecisionPercent));
	}

	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

protected:
	FString Name;
	int PrecisionPercent;
};

#undef LOCTEXT_NAMESPACE
