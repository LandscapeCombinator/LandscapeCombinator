// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMTilesRenamer.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMViewfinder15Renamer : public HMTilesRenamer
{
public:
	HMViewfinder15Renamer(FString LandscapeLabel0) : HMTilesRenamer(LandscapeLabel0) {};

	int TileToX(FString Tile) const override;
	int TileToY(FString Tile) const override;
};

#undef LOCTEXT_NAMESPACE