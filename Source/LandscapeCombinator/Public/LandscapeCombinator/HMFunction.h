// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMFetcher.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMFunction : public HMFetcher
{
public:
	HMFunction(FString LandscapeLabel0, TFunction<float(float)> Function0) :
		LandscapeLabel(LandscapeLabel0), Function(Function0) {};

	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString LandscapeLabel;
	TFunction<float(float)> Function;
};

#undef LOCTEXT_NAMESPACE
