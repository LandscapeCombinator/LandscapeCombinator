// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMFetcher.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMLocalFile : public HMFetcher
{
public:
	HMLocalFile(FString File0, int EPSG) : File(File0)
	{
		OutputEPSG = EPSG;
	};
	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString File;
	int OutputEPSG;
};

#undef LOCTEXT_NAMESPACE
