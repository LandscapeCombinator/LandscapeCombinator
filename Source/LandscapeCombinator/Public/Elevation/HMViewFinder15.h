// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMViewFinder.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMViewFinder15 : public HMViewFinder
{
protected:
	bool Initialize() override;

	int TileToX(FString Tile) const override;
	int TileToY(FString Tile) const override;

public:
	HMViewFinder15(FString LandscapeLabel0, const FText &KindText0, FString Descr0, int Precision0);
};

#undef LOCTEXT_NAMESPACE
