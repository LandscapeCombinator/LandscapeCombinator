// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMReadCRS : public HMFetcher
{
public:
	HMReadCRS() {};
	void Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;
};

#undef LOCTEXT_NAMESPACE
