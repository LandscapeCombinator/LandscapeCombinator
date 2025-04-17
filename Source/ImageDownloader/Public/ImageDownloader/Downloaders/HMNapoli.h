// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

class HMNapoli : public HMFetcher
{
public:
	HMNapoli(double MinLong0, double MaxLong0, double MinLat0, double MaxLat0)
	{
		MinLong = MinLong0;
		MaxLong = MaxLong0;
		MinLat = MinLat0;
		MaxLat = MaxLat0;
	};

	void OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	double MinLong;
	double MaxLong;
	double MinLat;
	double MaxLat;
};
