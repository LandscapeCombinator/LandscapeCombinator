// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ImageDownloader/HMFetcher.h"
#include "HMResolution.generated.h"

UENUM(BlueprintType)
enum class EGDALResamplingMethod : uint8
{
	Nearest UMETA(DisplayName = "Nearest Neighbour"),
	Bilinear,
	Cubic,
	CubicSpline UMETA(DisplayName = "Cubic Spline"),
	Lanczos,
	Average,
	RMS UMETA(DisplayName = "Root Mean Square"),
	Mode
};

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

class IMAGEDOWNLOADER_API HMResolution : public HMFetcher
{
public:
	HMResolution(FString Name0, FIntPoint Pixels0, EGDALResamplingMethod InResamplingMethod) :
		Name(MoveTemp(Name0)),
		Pixels(Pixels0),
		ResamplingMethod(InResamplingMethod)
	{};

	FString GetOutputDir() override
	{
		return FPaths::Combine(ImageDownloaderDir, FString::Printf(TEXT("%s-Resolution-%d"),
			*Name, static_cast<int32>(ResamplingMethod)));
	}

	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

private:
	static const TCHAR* GetGDALResamplingName(EGDALResamplingMethod Method);
	FString Name;
	FIntPoint Pixels;
	EGDALResamplingMethod ResamplingMethod;
};

#undef LOCTEXT_NAMESPACE
