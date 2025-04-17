// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"
#include "ConsoleHelpers/ExternalTool.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class HMPreprocess : public HMFetcher
{
public:
	HMPreprocess(FString Name0, TObjectPtr<UExternalTool> ExternalTool0) :
		Name(Name0), ExternalTool(ExternalTool0) {};

	FString GetOutputDir() override
	{
		return FPaths::Combine(ImageDownloaderDir, Name + "-Preprocess");
	}
	void OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete) override;

protected:
	FString Name;
	TObjectPtr<UExternalTool> ExternalTool;
};

#undef LOCTEXT_NAMESPACE
