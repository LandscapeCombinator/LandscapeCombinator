// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class IMAGEDOWNLOADER_API HMDebugFetcher : public HMFetcher
{
public:
	HMDebugFetcher(FString Name0, HMFetcher *Fetcher0, bool bAllowEmpty0 = false)
	{
		bAllowEmpty = bAllowEmpty0;
		Name = Name0;
		Fetcher = Fetcher0;
	};
	virtual ~HMDebugFetcher() { delete Fetcher; };

	FString Name;
	HMFetcher *Fetcher;
	bool bAllowEmpty;

	void SetIsUserInitiated(bool bIsUserInitiatedIn) override {
		HMFetcher::SetIsUserInitiated(bIsUserInitiatedIn);
		Fetcher->SetIsUserInitiated(bIsUserInitiatedIn);
	}
	
	void OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;
};

#undef LOCTEXT_NAMESPACE
