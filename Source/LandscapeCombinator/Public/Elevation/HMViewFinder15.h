// Copyright LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMInterfaceTiles.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMViewFinder15 : public HMInterfaceTiles
{
protected:
	bool Initialize() override;

	int TileToX(FString Tile) const override;
	int TileToY(FString Tile) const override;
	
	bool GetCoordinatesSpatialReference(OGRSpatialReference &InRs) const override;
	bool GetDataSpatialReference(OGRSpatialReference &InRs) const override;

public:
	HMViewFinder15(FString LandscapeLabel0, const FText &KindText0, FString Descr0, int Precision0);
};

#undef LOCTEXT_NAMESPACE
