// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMConvert : public HMFetcher
{
public:
	HMConvert(FString Name0, FString NewExtension0) :
		Name(Name0), NewExtension(NewExtension0) {};
	void Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString Name;
	FString NewExtension;
};

#undef LOCTEXT_NAMESPACE
