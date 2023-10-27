// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMTilesRenamer.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMDegreeRenamer : public HMTilesRenamer
{
public:
	HMDegreeRenamer(FString LandscapeLabel0) : HMTilesRenamer(LandscapeLabel0) {};

	int TileToX(FString Tile) const override;
	int TileToY(FString Tile) const override;
};

#undef LOCTEXT_NAMESPACE