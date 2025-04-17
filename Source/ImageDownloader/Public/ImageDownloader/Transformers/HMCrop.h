// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class IMAGEDOWNLOADER_API HMCrop : public HMFetcher
{
public:
	HMCrop(FString Name0, FVector4d Coordinates0, FIntPoint Pixels0) :
		Name(Name0),
		Coordinates(Coordinates0),
		Pixels(Pixels0)
	{};

	FString GetOutputDir() override
	{
		return FPaths::Combine(ImageDownloaderDir, Name + "-Crop");
	}
	void OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

private:
	FString Name;
	FVector4d Coordinates;
	FIntPoint Pixels;
};

#undef LOCTEXT_NAMESPACE
