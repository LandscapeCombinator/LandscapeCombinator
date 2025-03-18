// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMLitto3DGuadeloupe: public HMFetcher
{
public:
	HMLitto3DGuadeloupe(FString Folder0, bool bUse5mData0, bool bSkipExtraction0) :
		Folder(Folder0), bUse5mData(bUse5mData0), bSkipExtraction(bSkipExtraction0)
	{
		OutputCRS = "EPSG:4559";
	};
	void Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString Folder;
	bool bUse5mData;
	bool bSkipExtraction;
};

#undef LOCTEXT_NAMESPACE