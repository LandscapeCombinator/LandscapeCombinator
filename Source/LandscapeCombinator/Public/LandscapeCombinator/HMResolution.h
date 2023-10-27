// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMFetcher.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMResolution : public HMFetcher
{
public:
	HMResolution(FString LandscapeLabel0, int PrecisionPercent0) : LandscapeLabel(LandscapeLabel0), PrecisionPercent(PrecisionPercent0) {};
	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString LandscapeLabel;
	int PrecisionPercent;
};

#undef LOCTEXT_NAMESPACE
