// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMFetcher.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMLocalFile : public HMFetcher
{
public:
	HMLocalFile(FString File0) : File(File0) {};
	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString File;
};

#undef LOCTEXT_NAMESPACE
