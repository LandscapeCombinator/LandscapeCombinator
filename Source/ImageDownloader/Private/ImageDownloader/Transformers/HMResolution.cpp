// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMResolution.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "HAL/PlatformFile.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

const TCHAR* HMResolution::GetGDALResamplingName(EGDALResamplingMethod Method)
{
	switch (Method)
	{
	case EGDALResamplingMethod::Nearest: return TEXT("near");
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

bool HMResolution::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	OutputCRS = InputCRS;
	const TCHAR* ResamplingName = GetGDALResamplingName(ResamplingMethod);
	if (!ResamplingName)
	{
		UE_LOG(LogImageDownloader, Error, TEXT("Adapt Resolution requires a valid GDAL resampling method."));
		return false;
	}

	if (InputFiles.Num() != 1)
	{
		LCReporter::ShowError(
			LOCTEXT("HMResolution::Fetch", "Image Downloader Error: The AdaptResolution option is not available for sources that are made of multiple images, use Merge Images option on Heightmap Downloader and in the Decals settings.")
		);
		return false;
	}

	FString InputFile = InputFiles[0];
	FString CroppedFile = FPaths::Combine(OutputDir, FPaths::GetBaseFilename(InputFile) + ".tif");
	OutputFiles.Add(CroppedFile);

	TArray<FString> Args;
	Args.Add("-s_srs");
	Args.Add(InputCRS);
	Args.Add("-t_srs");
	Args.Add(InputCRS);
	Args.Add("-r");
	Args.Add(ResamplingName);

	if (Pixels != FIntPoint::ZeroValue)
	{
		Args.Add("-ts");
		Args.Add(FString::FromInt(Pixels[0]));
		Args.Add(FString::FromInt(Pixels[1]));
	}

	if (!GDALInterface::Warp(InputFile, CroppedFile, Args)) return false;

	return true;
}

#undef LOCTEXT_NAMESPACE
