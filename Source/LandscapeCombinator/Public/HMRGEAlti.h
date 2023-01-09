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

	bool Initialize() override;
	bool GetInsidePixels(FIntPoint &InsidePixels) const override;
	bool GetSpatialReference(OGRSpatialReference &InRs) const override;

public:
	HMRGEAlti(FString LandscapeName0, const FText &KindText0, FString Descr0, int Precision0);

	FReply DownloadHeightMapsImpl() const override;
};

#undef LOCTEXT_NAMESPACE
