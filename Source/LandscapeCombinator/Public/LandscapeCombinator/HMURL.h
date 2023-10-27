// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMFetcher.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMURL : public HMFetcher
{
public:
	HMURL(FString URL0, FString FileName0) : URL(URL0), FileName(FileName0) {};
	void Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

protected:
	FString URL;
	FString FileName;
};

#undef LOCTEXT_NAMESPACE
