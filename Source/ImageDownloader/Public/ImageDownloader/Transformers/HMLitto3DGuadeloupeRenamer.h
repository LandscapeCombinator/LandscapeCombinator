// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HMTilesRenamer.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMLitto3DGuadeloupeRenamer : public HMTilesRenamer
{
public:
	HMLitto3DGuadeloupeRenamer(FString Name0) : HMTilesRenamer(Name0) {};

	int TileToX(FString Tile) const override;
	int TileToY(FString Tile) const override;
};

#undef LOCTEXT_NAMESPACE
