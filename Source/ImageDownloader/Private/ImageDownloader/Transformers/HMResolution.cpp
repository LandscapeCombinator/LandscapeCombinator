// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMResolution.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "HAL/PlatformFile.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMResolution::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	OutputCRS = InputCRS;

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