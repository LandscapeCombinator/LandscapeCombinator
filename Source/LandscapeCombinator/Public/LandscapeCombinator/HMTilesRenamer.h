// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMFetcher.h"
#include "HMTilesCounter.h"
#include "CoreMinimal.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMTilesRenamer : public HMFetcher, public HMTilesCounter
{
public:
	HMTilesRenamer(FString LandscapeLabel0) : LandscapeLabel(LandscapeLabel0) {};
	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete);

protected:
	FString LandscapeLabel;

private:
	FString Rename(FString Tile) const;
};

#undef LOCTEXT_NAMESPACE