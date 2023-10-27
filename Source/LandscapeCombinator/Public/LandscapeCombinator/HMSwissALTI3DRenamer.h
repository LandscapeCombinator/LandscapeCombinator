// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMTilesRenamer.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMSwissALTI3DRenamer : public HMTilesRenamer
{
public:
	HMSwissALTI3DRenamer(FString LandscapeLabel0) : HMTilesRenamer(LandscapeLabel0) {};

	int TileToX(FString Tile) const override;
	int TileToY(FString Tile) const override;
};

#undef LOCTEXT_NAMESPACE