// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

class HMListDownloader : public HMFetcher
{
public:
	HMListDownloader(FString ListOfLinks0) : ListOfLinks(ListOfLinks0) {};

	void OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString ListOfLinks;
};
