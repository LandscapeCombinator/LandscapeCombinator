// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMViewfinderDownloader: public HMFetcher
{
public:
	HMViewfinderDownloader(FString MegaTilesString, FString BaseURL, bool bIs15);

	virtual bool ValidateTiles() const { return true; };
	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

protected:
	TArray<FString> MegaTiles;
	FString BaseURL;
	bool bIs15;
};

#undef LOCTEXT_NAMESPACE