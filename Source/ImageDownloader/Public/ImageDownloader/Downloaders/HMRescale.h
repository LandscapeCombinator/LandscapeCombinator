#pragma once

#include "CoreMinimal.h"
#include "ImageDownloader/HMFetcher.h"

#include "HMRescale.generated.h"

UENUM(BlueprintType)
enum class EMapResamplingMethod : uint8
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

/** Resizes each input raster using an explicit GDAL resampling method. */
class HMRescale : public HMFetcher
{
public:
	HMRescale(FString InName, int32 InPrecisionPercent, EMapResamplingMethod InResamplingMethod)
		: Name(MoveTemp(InName)), PrecisionPercent(InPrecisionPercent), ResamplingMethod(InResamplingMethod) {}

protected:
	virtual FString GetOutputDir() override;
	virtual bool OnFetch(FString InputCRS, TArray<FString> InputFiles) override;

private:
	static const TCHAR* GetGDALResamplingName(EMapResamplingMethod Method);
	FString Name;
	int32 PrecisionPercent;
	EMapResamplingMethod ResamplingMethod;
};
