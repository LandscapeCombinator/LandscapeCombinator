// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMConvert : public HMFetcher
{
public:
	HMConvert(FString Name0, FString NewExtension0) :
		Name(Name0), NewExtension(NewExtension0) {};

	FString GetOutputDir() override
	{
		return FPaths::Combine(ImageDownloaderDir, Name + "-Convert");
	}
	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

protected:
	FString Name;
	FString NewExtension;
};

#undef LOCTEXT_NAMESPACE
