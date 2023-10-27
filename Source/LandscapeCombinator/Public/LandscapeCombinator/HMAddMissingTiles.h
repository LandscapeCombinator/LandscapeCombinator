// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMFetcher.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMAddMissingTiles : public HMFetcher
{
public:
	HMAddMissingTiles() {};
	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;
};

#undef LOCTEXT_NAMESPACE
