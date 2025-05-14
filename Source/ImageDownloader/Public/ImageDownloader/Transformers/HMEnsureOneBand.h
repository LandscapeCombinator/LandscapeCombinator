// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMEnsureOneBand : public HMFetcher
{
public:
	HMEnsureOneBand() {};

	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;
};

#undef LOCTEXT_NAMESPACE
