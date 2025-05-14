// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMAddMissingTiles : public HMFetcher
{
public:
	HMAddMissingTiles() {};

	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;
};

#undef LOCTEXT_NAMESPACE
