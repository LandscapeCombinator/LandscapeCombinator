// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMTilesRenamer.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMViewfinder15Renamer : public HMTilesRenamer
{
public:
	HMViewfinder15Renamer(FString Name0) : HMTilesRenamer(Name0) {};

	int TileToX(FString Tile) const override;
	int TileToY(FString Tile) const override;
};

#undef LOCTEXT_NAMESPACE