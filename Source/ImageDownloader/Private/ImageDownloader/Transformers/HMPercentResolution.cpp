// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMPercentResolution.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"

#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/Concurrency.h"

#include "HAL/PlatformFile.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

const TCHAR* HMPercentResolution::GetGDALResamplingName(EGDALResamplingMethod Method)
{
	switch (Method)
	{
	case EGDALResamplingMethod::Nearest: return TEXT("nearest");
	case EGDALResamplingMethod::Bilinear: return TEXT("bilinear");
	case EGDALResamplingMethod::Cubic: return TEXT("cubic");
	case EGDALResamplingMethod::CubicSpline: return TEXT("cubicspline");
	case EGDALResamplingMethod::Lanczos: return TEXT("lanczos");
	case EGDALResamplingMethod::Average: return TEXT("average");
	case EGDALResamplingMethod::RMS: return TEXT("rms");
	case EGDALResamplingMethod::Mode: return TEXT("mode");
	default: return nullptr;
	}
}

bool HMPercentResolution::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	OutputCRS = InputCRS;
	const TCHAR* ResamplingName = GetGDALResamplingName(ResamplingMethod);
	if (!ResamplingName)
	{
		UE_LOG(LogImageDownloader, Error, TEXT("Scale Resolution requires a valid GDAL resampling method."));
		return false;
	}
	for (int32 i = 0; i < InputFiles.Num(); i++)
	{
		FString InputFile = InputFiles[i];
		FString OutputFile = FPaths::Combine(OutputDir, FPaths::GetCleanFilename(InputFile));
		OutputFiles.Add(OutputFile);

		if (!GDALInterface::ChangeResolution(InputFile, OutputFile, PrecisionPercent, ResamplingName)) return false;
	}
	return true;
}


#undef LOCTEXT_NAMESPACE