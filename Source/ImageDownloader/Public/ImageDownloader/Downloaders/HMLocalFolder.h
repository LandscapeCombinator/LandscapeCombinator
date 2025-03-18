// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMLocalFolder : public HMFetcher
{
public:
	HMLocalFolder(FString Folder0, FString CRS) : Folder(Folder0)
	{
		OutputCRS = CRS;
	};
	void Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString Folder;
};

#undef LOCTEXT_NAMESPACE
