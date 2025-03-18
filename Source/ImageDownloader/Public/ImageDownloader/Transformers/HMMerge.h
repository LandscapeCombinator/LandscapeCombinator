// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class IMAGEDOWNLOADER_API HMMerge : public HMFetcher
{
public:
	HMMerge(FString Name0) : Name(Name0) {}
	void Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString Name;
};

#undef LOCTEXT_NAMESPACE
