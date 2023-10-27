// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMFetcher.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMToPNG : public HMFetcher
{
public:
	HMToPNG(FString LandscapeLabel0) : LandscapeLabel(LandscapeLabel0) {};
	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString LandscapeLabel;
};

#undef LOCTEXT_NAMESPACE
