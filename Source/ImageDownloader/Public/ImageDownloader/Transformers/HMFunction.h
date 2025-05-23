// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMFunction : public HMFetcher
{
public:
	HMFunction(TFunction<float(float)> Function0) :
		Function(Function0) {};

	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

private:
	TFunction<float(float)> Function;
};

#undef LOCTEXT_NAMESPACE
