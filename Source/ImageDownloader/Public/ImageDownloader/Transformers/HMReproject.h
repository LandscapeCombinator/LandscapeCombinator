// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMReproject : public HMFetcher
{
public:
	HMReproject(FString Name0, FString CRSReprojection, bool bCrop0 = true) : Name(Name0), bCrop(bCrop0)
	{
		OutputCRS = CRSReprojection;
	};


	FString GetOutputDir() override
	{
		return FPaths::Combine(
			ImageDownloaderDir,
			Name + "-" + OutputCRS.Replace(TEXT(":"), TEXT("_"))
		);
	}

	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

private:
	FString Name;
	bool bCrop;
};

#undef LOCTEXT_NAMESPACE
