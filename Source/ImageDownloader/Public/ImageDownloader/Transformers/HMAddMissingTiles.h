// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMAddMissingTiles : public HMFetcher
{
public:
	HMAddMissingTiles() {};

	void OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;
};

#undef LOCTEXT_NAMESPACE
