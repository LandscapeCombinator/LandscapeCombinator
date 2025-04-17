// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMResolution : public HMFetcher
{
public:
	HMResolution(FString Name0, int PrecisionPercent0) : Name(Name0), PrecisionPercent(PrecisionPercent0) {};
	
	FString GetOutputDir() override
	{
		return FPaths::Combine(ImageDownloaderDir, Name + "-Resolution" + FString::FromInt(PrecisionPercent));
	}

	void OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

protected:
	FString Name;
	int PrecisionPercent;
};

#undef LOCTEXT_NAMESPACE
