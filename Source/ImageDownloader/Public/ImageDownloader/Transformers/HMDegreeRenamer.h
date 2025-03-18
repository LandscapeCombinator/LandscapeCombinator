// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMTilesRenamer.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMDegreeRenamer : public HMTilesRenamer
{
public:
	HMDegreeRenamer(FString Name0) : HMTilesRenamer(Name0) {};

	int TileToX(FString Tile) const override;
	int TileToY(FString Tile) const override;
};

#undef LOCTEXT_NAMESPACE