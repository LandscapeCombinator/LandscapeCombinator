// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMFetcher.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMSetEPSG : public HMFetcher
{
public:
	HMSetEPSG() {};
	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;
};

#undef LOCTEXT_NAMESPACE
