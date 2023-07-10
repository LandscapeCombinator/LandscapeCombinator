// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMInterface.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMReprojected : public HMInterface
{
protected:
	HMInterface *Target;
	bool Initialize() override;
	bool GetCoordinatesSpatialReference(OGRSpatialReference &InRs) const override;
	bool GetDataSpatialReference(OGRSpatialReference &InRs) const override;

public:
	HMReprojected(FString LandscapeLabel0, const FText &KindText0, FString Descr0, int Precision0, HMInterface *Target0);

	FReply DownloadHeightMapsImpl(TFunction<void(bool)> OnComplete) const override;
};

#undef LOCTEXT_NAMESPACE
