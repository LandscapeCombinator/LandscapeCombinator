// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HMFetcher.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMViewfinderDownloader: public HMFetcher
{
public:
	HMViewfinderDownloader(FString MegaTilesString, FString BaseURL, bool bIs15);

	virtual bool ValidateTiles() const { return true; };
	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

protected:
	TArray<FString> MegaTiles;
	FString BaseURL;
	bool bIs15;
};

#undef LOCTEXT_NAMESPACE