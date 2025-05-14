// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMDegreeRenamer.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMDegreeFilter : public HMDegreeRenamer
{
public:
	HMDegreeFilter(FString Name0, int MinLong0, int MaxLong0, int MinLat0, int MaxLat0) :
		HMDegreeRenamer(Name0),
		MinLong(MinLong0),
		MaxLong(MaxLong0),
		MinLat(MinLat0),
		MaxLat(MaxLat0)
	{};

	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

private:
	int MinLong;
	int MaxLong;
	int MinLat;
	int MaxLat;
};

#undef LOCTEXT_NAMESPACE