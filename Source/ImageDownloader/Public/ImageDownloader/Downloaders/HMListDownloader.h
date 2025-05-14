// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

class HMListDownloader : public HMFetcher
{
public:
	HMListDownloader(FString ListOfLinks0) : ListOfLinks(ListOfLinks0) {};

	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

private:
	FString ListOfLinks;
};
