// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMFetcher.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMLocalFolder : public HMFetcher
{
public:
	HMLocalFolder(FString Folder0) : Folder(Folder0) {};
	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString Folder;
};

#undef LOCTEXT_NAMESPACE
