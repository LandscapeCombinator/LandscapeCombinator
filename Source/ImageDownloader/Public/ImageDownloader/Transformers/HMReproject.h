// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMReproject : public HMFetcher
{
public:
	HMReproject(FString Name0, FString CRSReprojection) : Name(Name0)
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

	void OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString Name;
};

#undef LOCTEXT_NAMESPACE
