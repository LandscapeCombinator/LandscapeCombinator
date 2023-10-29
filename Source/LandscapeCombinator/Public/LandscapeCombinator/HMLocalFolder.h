// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMFetcher.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMLocalFolder : public HMFetcher
{
public:
	HMLocalFolder(FString Folder0, int EPSG) : Folder(Folder0)
	{
		OutputEPSG = EPSG;
	};
	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString Folder;
	int OutputEPSG;
};

#undef LOCTEXT_NAMESPACE
