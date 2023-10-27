// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMFetcher.h"

class HMListDownloader : public HMFetcher
{
public:
	HMListDownloader(FString ListOfLinks0) : ListOfLinks(ListOfLinks0) {};	
	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString ListOfLinks;
};
