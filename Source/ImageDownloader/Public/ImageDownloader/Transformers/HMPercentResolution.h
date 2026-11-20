// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ImageDownloader/HMFetcher.h"
#include "HMPercentResolution.generated.h"

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

class HMPercentResolution : public HMFetcher
{
public:
	HMPercentResolution(FString Name0, int PrecisionPercent0, EGDALResamplingMethod InResamplingMethod)
		: Name(MoveTemp(Name0)), PrecisionPercent(PrecisionPercent0), ResamplingMethod(InResamplingMethod) {}
	
	FString GetOutputDir() override
	{
		return FPaths::Combine(ImageDownloaderDir, FString::Printf(TEXT("%s-PercentResolution%d-%d"),
			*Name, PrecisionPercent, static_cast<int32>(ResamplingMethod)));
	}

	bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

protected:
	static const TCHAR* GetGDALResamplingName(EGDALResamplingMethod Method);
	FString Name;
	int PrecisionPercent;
	EGDALResamplingMethod ResamplingMethod;
};

#undef LOCTEXT_NAMESPACE
