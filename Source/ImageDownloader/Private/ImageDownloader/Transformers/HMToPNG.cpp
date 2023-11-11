// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMToPNG.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"

#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMToPNG::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputCRS = InputCRS;
	FString PNGFolder = FPaths::Combine(Directories::ImageDownloaderDir(), Name + "-PNG");

	if (!IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*PNGFolder) || !IPlatformFile::GetPlatformPhysical().CreateDirectory(*PNGFolder))
	{
		Directories::CouldNotInitializeDirectory(PNGFolder);
		if (OnComplete) OnComplete(false);
		return;
	}

	FVector2D Altitudes;
	if (bScaleAltitude && !GDALInterface::GetMinMax(Altitudes, InputFiles))
	{
		if (OnComplete) OnComplete(false);
		return;
	}

	double MinAltitude = Altitudes[0];
	double MaxAltitude = Altitudes[1];

	FScopedSlowTask ToPNGTask(InputFiles.Num(), LOCTEXT("ToPNGTask", "GDAL Interface: Translating Files to PNG"));
	ToPNGTask.MakeDialog(true);

	for (int32 i = 0; i < InputFiles.Num(); i++)
	{
		if (ToPNGTask.ShouldCancel())
		{
			if (OnComplete) OnComplete(false);
			return;
		}
		ToPNGTask.EnterProgressFrame(1);

		FString InputFile = InputFiles[i];
		FString PNGFile = FPaths::Combine(PNGFolder, FPaths::GetBaseFilename(InputFile) + ".png");
		OutputFiles.Add(PNGFile);

		if (bScaleAltitude)
		{
			if (!GDALInterface::ConvertToPNG(InputFile, PNGFile, MinAltitude, MaxAltitude))
			{
				if (OnComplete) OnComplete(false);
				return;
			}
		}
		else
		{
			if (!GDALInterface::ConvertToPNG(InputFile, PNGFile, 0, 255))
			{
				if (OnComplete) OnComplete(false);
				return;
			}
		}
	}

	if (OnComplete) OnComplete(true);
	return;
}

#undef LOCTEXT_NAMESPACE