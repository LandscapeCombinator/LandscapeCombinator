// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"
#include "ImageDownloader/TilesCounter.h"
#include "CoreMinimal.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMTilesRenamer : public HMFetcher, public TilesCounter
{
public:
	HMTilesRenamer(FString Name0) : Name(Name0) {};
	
	FString GetOutputDir() override
	{
		return FPaths::Combine(ImageDownloaderDir, Name + "-Renamer");
	}

	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

protected:
	FString Name;
	FString Rename(FString Tile) const;
};

#undef LOCTEXT_NAMESPACE