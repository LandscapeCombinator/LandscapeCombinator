#pragma once

#include "HMInterface.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMElevationAPI : public HMInterface
{
protected:
	FString JSONFile;
	
	double MinLong;
	double MaxLong;
	double MinLat;
	double MaxLat;
	
	bool Initialize() override;
	bool GetMinMax(FVector2D &MinMax) const override;
	bool GetCoordinates(FVector4d &Coordinates) const override;
	bool GetSpatialReference(OGRSpatialReference &InRs) const override;

public:
	HMElevationAPI(FString LandscapeName0, const FText &KindText0, FString Descr0, int Precision0);

	FReply DownloadHeightMapsImpl() const override;
};

#undef LOCTEXT_NAMESPACE
