// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMInterfaceTiles.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMSwissALTI3D : public HMInterfaceTiles
{
protected:
	bool Initialize() override;
	int TileToX(FString Tile) const override;
	int TileToY(FString Tile) const override;
	
	bool GetCoordinatesSpatialReference(OGRSpatialReference &InRs) const override;
	bool GetDataSpatialReference(OGRSpatialReference &InRs) const override;

public:
	HMSwissALTI3D(FString LandscapeLabel0, const FText &KindText0, FString Descr0, int Precision0);

	FReply DownloadHeightMapsImpl(TFunction<void(bool)> OnComplete) const override;
};

#undef LOCTEXT_NAMESPACE