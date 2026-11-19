#include "ImageDownloader/Downloaders/HMRescale.h"

#include "GDALInterface/GDALInterface.h"
#include "ImageDownloader/LogImageDownloader.h"

const TCHAR* HMRescale::GetGDALResamplingName(EMapResamplingMethod Method)
{
	switch (Method)
	{
	case EMapResamplingMethod::Nearest: return TEXT("nearest");
	case EMapResamplingMethod::Bilinear: return TEXT("bilinear");
	case EMapResamplingMethod::Cubic: return TEXT("cubic");
	case EMapResamplingMethod::CubicSpline: return TEXT("cubicspline");
	case EMapResamplingMethod::Lanczos: return TEXT("lanczos");
	case EMapResamplingMethod::Average: return TEXT("average");
	case EMapResamplingMethod::RMS: return TEXT("rms");
	case EMapResamplingMethod::Mode: return TEXT("mode");
	default: return nullptr;
	}
}

FString HMRescale::GetOutputDir()
{
	return FPaths::Combine(ImageDownloaderDir, FString::Printf(TEXT("%s-Rescale-%d-%d"),
		*Name, PrecisionPercent, static_cast<int32>(ResamplingMethod)));
}

bool HMRescale::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	OutputFiles.Reset();
	OutputCRS = InputCRS;
	const TCHAR* ResamplingName = GetGDALResamplingName(ResamplingMethod);
	if (PrecisionPercent <= 0 || !ResamplingName)
	{
		UE_LOG(LogImageDownloader, Error, TEXT("Map rescaling requires a positive percentage and a valid resampling method."));
		return false;
	}

	const FString Percent = FString::Printf(TEXT("%d%%"), PrecisionPercent);
	const TArray<FString> Args = {TEXT("-outsize"), Percent, Percent, TEXT("-r"), ResamplingName};
	for (const FString& InputFile : InputFiles)
	{
		const FString OutputFile = FPaths::Combine(OutputDir, FPaths::GetCleanFilename(InputFile));
		if (!GDALInterface::Translate(InputFile, OutputFile, Args))
		{
			return false;
		}
		OutputFiles.Add(OutputFile);
	}
	return true;
}
