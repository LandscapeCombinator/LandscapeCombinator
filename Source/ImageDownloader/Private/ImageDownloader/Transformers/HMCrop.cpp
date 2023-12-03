// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMCrop.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"

#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMCrop::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputCRS = InputCRS;

	if (InputFiles.Num() != 1)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("HMCrop::Fetch", "Image Downloader Error: The AdaptResolution and CropCoordinates options are not available for sources that are made of multiple images.")
		);
		if (OnComplete) OnComplete(false);
		return;
	}

	FString CropFolder = FPaths::Combine(Directories::ImageDownloaderDir(), Name + "-Crop");

	if (!IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*CropFolder) || !IPlatformFile::GetPlatformPhysical().CreateDirectory(*CropFolder))
	{
		Directories::CouldNotInitializeDirectory(CropFolder);
		if (OnComplete) OnComplete(false);
		return;
	}

	double South = Coordinates[2];
	double West = Coordinates[0];
	double North = Coordinates[3];
	double East = Coordinates[1];

	FScopedSlowTask CropTask(InputFiles.Num(), LOCTEXT("CropTask", "GDAL Interface: Cropping files"));
	CropTask.MakeDialog(true);

	for (int32 i = 0; i < InputFiles.Num(); i++)
	{
		if (CropTask.ShouldCancel())
		{
			if (OnComplete) OnComplete(false);
			return;
		}
		CropTask.EnterProgressFrame(1);

		FString InputFile = InputFiles[i];
		FString CroppedFile = FPaths::Combine(CropFolder, FPaths::GetBaseFilename(InputFile) + ".tif");
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

		if (!GDALInterface::Warp(InputFile, CroppedFile, Args))
		{
			if (OnComplete) OnComplete(false);
			return;
		}
	}

	if (OnComplete) OnComplete(true);
	return;
}

#undef LOCTEXT_NAMESPACE