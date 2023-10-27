// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMFetcher.h"
#include "ConsoleHelpers/ExternalTool.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMPreprocess : public HMFetcher
{
public:
	HMPreprocess(FString LandscapeLabel0, TObjectPtr<UExternalTool> ExternalTool0) :
		LandscapeLabel(LandscapeLabel0), ExternalTool(ExternalTool0) {};

	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString LandscapeLabel;
	TObjectPtr<UExternalTool> ExternalTool;
};

#undef LOCTEXT_NAMESPACE
