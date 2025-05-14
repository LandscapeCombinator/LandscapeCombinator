// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMLocalFile : public HMFetcher
{
public:
	HMLocalFile(FString File0, FString CRS) : File(File0)
	{
		OutputCRS = CRS;
	};
	
	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

private:
	FString File;
};

#undef LOCTEXT_NAMESPACE
