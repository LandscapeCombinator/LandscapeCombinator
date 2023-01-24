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
	bool GetCoordinatesSpatialReference(OGRSpatialReference &InRs) const override;
	bool GetDataSpatialReference(OGRSpatialReference &InRs) const override;

public:
	HMElevationAPI(FString LandscapeName0, const FText &KindText0, FString Descr0, int Precision0);

	FReply DownloadHeightMapsImpl(TFunction<void(bool)> OnComplete) const override;
};

#undef LOCTEXT_NAMESPACE
