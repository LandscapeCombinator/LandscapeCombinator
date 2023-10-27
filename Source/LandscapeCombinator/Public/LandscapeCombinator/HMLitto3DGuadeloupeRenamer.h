// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HMTilesRenamer.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMLitto3DGuadeloupeRenamer : public HMTilesRenamer
{
public:
	HMLitto3DGuadeloupeRenamer(FString LandscapeLabel0) : HMTilesRenamer(LandscapeLabel0) {};

	int TileToX(FString Tile) const override;
	int TileToY(FString Tile) const override;
};

#undef LOCTEXT_NAMESPACE
