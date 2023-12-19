// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMDecodeMapbox : public HMFetcher
{
public:
	HMDecodeMapbox(FString Name0) : Name(Name0) {};

	void Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString Name;
};

#undef LOCTEXT_NAMESPACE
