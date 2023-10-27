// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMFetcher.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMReproject : public HMFetcher
{
public:
	HMReproject(FString LandscapeLabel0, int EPSGReprojection) : LandscapeLabel(LandscapeLabel0)
	{
		OutputEPSG = EPSGReprojection;
	};
	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString LandscapeLabel;
};

#undef LOCTEXT_NAMESPACE
