// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HMFetcher.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMDebugFetcher : public HMFetcher
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
	
	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;
};

#undef LOCTEXT_NAMESPACE
