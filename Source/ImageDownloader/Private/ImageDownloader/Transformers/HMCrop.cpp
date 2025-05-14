// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMCrop.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "HAL/PlatformFile.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMCrop::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	OutputCRS = InputCRS;

	if (InputFiles.Num() != 1)
	{
		LCReporter::ShowError(
			LOCTEXT("HMCrop::Fetch", "Image Downloader Error: The AdaptResolution and CropCoordinates options are not available for sources that are made of multiple images.")
		);
		return false;
	}

	double South = Coordinates[2];
	double West = Coordinates[0];
	double North = Coordinates[3];
	double East = Coordinates[1];

	for (int32 i = 0; i < InputFiles.Num(); i++)
	{
		FString InputFile = InputFiles[i];
		FString CroppedFile = FPaths::Combine(OutputDir, FPaths::GetBaseFilename(InputFile) + ".tif");
		OutputFiles.Add(CroppedFile);

		TArray<FString> Args;
		Args.Add("-s_srs");
		Args.Add(InputCRS);
		Args.Add("-t_srs");
		Args.Add(InputCRS);

		if (Coordinates != FVector4d::Zero())
		{
			Args.Add("-te");
			Args.Add(FString::SanitizeFloat(West));
			Args.Add(FString::SanitizeFloat(South));
			Args.Add(FString::SanitizeFloat(East));
			Args.Add(FString::SanitizeFloat(North));
		}

		if (Pixels != FIntPoint::ZeroValue)
		{
			Args.Add("-ts");
			Args.Add(FString::FromInt(Pixels[0]));
			Args.Add(FString::FromInt(Pixels[1]));
		}

		if (!GDALInterface::Warp(InputFile, CroppedFile, Args)) return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE