#pragma once

#include "HMInterface.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMLocalFile : public HMInterface
{
protected:
	bool Initialize() override;
	bool GetSpatialReference(OGRSpatialReference &InRs) const override;

public:
	HMLocalFile(FString LandscapeName0, const FText &KindText0, FString Descr0, int Precision0);

	FReply DownloadHeightMapsImpl() const override;
};

#undef LOCTEXT_NAMESPACE
