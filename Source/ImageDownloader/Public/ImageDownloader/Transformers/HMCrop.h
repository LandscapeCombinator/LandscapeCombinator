// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

class IMAGEDOWNLOADER_API HMCrop : public HMFetcher
{
public:
	HMCrop(FString Name0, FVector4d Coordinates0) :
		Name(Name0),
		Coordinates(Coordinates0)
	{};

	FString GetOutputDir() override
	{
		return FPaths::Combine(ImageDownloaderDir, Name + "-Crop");
	}
	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

private:
	FString Name;
	FVector4d Coordinates;
};
