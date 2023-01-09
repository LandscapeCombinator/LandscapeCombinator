#pragma once

#include "HMInterface.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMURL : public HMInterface
{
protected:
	bool Initialize() override;
	bool GetSpatialReference(OGRSpatialReference &InRs) const override;

public:
	HMURL(FString LandscapeName0, const FText &KindText0, FString Descr0, int Precision0);

	FReply DownloadHeightMapsImpl() const override;
};

#undef LOCTEXT_NAMESPACE
