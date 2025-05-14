// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class IMAGEDOWNLOADER_API HMMerge : public HMFetcher
{
public:
	HMMerge(FString Name0) : Name(Name0) {}

	FString GetOutputDir() override
	{
		return FPaths::Combine(ImageDownloaderDir, Name + "-Merge");
	}
	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

private:
	FString Name;
};

#undef LOCTEXT_NAMESPACE
