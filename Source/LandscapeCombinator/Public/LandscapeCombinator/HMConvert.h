// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMFetcher.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMConvert : public HMFetcher
{
public:
	HMConvert(FString LandscapeLabel0, FString NewExtension0) :
		LandscapeLabel(LandscapeLabel0), NewExtension(NewExtension0) {};
	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString LandscapeLabel;
	FString NewExtension;
};

#undef LOCTEXT_NAMESPACE
