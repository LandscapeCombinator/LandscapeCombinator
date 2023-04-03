// Copyright LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMInterface.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMRGEAlti : public HMInterface
{
protected:	
	double MinLong;
	double MaxLong;
	double MinLat;
	double MaxLat;

	int Width;
	int Height;

	bool Initialize() override;
	bool GetInsidePixels(FIntPoint &InsidePixels) const override;
	bool GetCoordinatesSpatialReference(OGRSpatialReference &InRs) const override;
	bool GetDataSpatialReference(OGRSpatialReference &InRs) const override;

public:
	HMRGEAlti(FString LandscapeLabel0, const FText &KindText0, FString Descr0, int Precision0);

	FReply DownloadHeightMapsImpl(TFunction<void(bool)> OnComplete) const override;
};

#undef LOCTEXT_NAMESPACE
