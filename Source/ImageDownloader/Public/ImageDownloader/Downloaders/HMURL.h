// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMURL : public HMFetcher
{
public:
	HMURL(FString URL0, FString FileName0, FString CRS) : URL(URL0), FileName(FileName0)
	{
		OutputCRS = CRS;
	};
	void Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

protected:
	FString URL;
	FString FileName;
};

#undef LOCTEXT_NAMESPACE
