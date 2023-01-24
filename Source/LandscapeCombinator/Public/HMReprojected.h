#pragma once

#include "HMLocalFile.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMReprojected : public HMLocalFile
{
protected:
	HMInterface *Target;
	bool Initialize() override;

public:
	HMReprojected(FString LandscapeName0, const FText &KindText0, FString Descr0, int Precision0, HMInterface *Target0);

	FReply DownloadHeightMapsImpl(TFunction<void(bool)> OnComplete) const override;
};

#undef LOCTEXT_NAMESPACE
