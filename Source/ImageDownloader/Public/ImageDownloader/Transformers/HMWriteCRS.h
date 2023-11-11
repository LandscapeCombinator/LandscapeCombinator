// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMWriteCRS : public HMFetcher
{
public:
	HMWriteCRS(FString Name0, int Width0, int Height0, double MinLong0, double MaxLong0, double MinLat0, double MaxLat0)
	{
		Name = Name0;
		Width = Width0;
		Height = Height0;
		MinLong = MinLong0;
		MaxLong = MaxLong0;
		MinLat = MinLat0;
		MaxLat = MaxLat0;
	};
	void Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString Name;

	int Width;
	int Height;
	double MinLong;
	double MaxLong;
	double MinLat;
	double MaxLat;
};

#undef LOCTEXT_NAMESPACE
