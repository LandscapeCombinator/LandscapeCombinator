// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class IMAGEDOWNLOADER_API HMResolution : public HMFetcher
{
public:
	HMResolution(FString Name0, FIntPoint Pixels0) :
		Name(Name0),
		Pixels(Pixels0)
	{};

	FString GetOutputDir() override
	{
		return FPaths::Combine(ImageDownloaderDir, Name + "-Resolution");
	}

	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

private:
	FString Name;
	FIntPoint Pixels;
};

#undef LOCTEXT_NAMESPACE
