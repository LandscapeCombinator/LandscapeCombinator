// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"
#include "ImageDownloader/TilesCounter.h"
#include "CoreMinimal.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMTilesRenamer : public HMFetcher, public TilesCounter
{
public:
	HMTilesRenamer(FString Name0) : Name(Name0) {};
	void Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete);

protected:
	FString Name;

private:
	FString Rename(FString Tile) const;
};

#undef LOCTEXT_NAMESPACE