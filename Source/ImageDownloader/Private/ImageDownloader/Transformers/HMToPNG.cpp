// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMToPNG.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"

#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMToPNG::OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputCRS = InputCRS;

	FVector2D Altitudes;
	if (bScaleAltitude && !GDALInterface::GetMinMax(Altitudes, InputFiles))
	{
		if (OnComplete) OnComplete(false);
		return;
	}

	double MinAltitude = Altitudes[0];
	double MaxAltitude = Altitudes[1];

	FScopedSlowTask ToPNGTask(InputFiles.Num(), LOCTEXT("ToPNGTask", "GDAL Interface: Translating Files to PNG"));

	if (InputFiles.Num() > 10)
	{
		ToPNGTask.MakeDialog(true);
	}

	for (int32 i = 0; i < InputFiles.Num(); i++)
	{
		if (ToPNGTask.ShouldCancel())
		{
			if (OnComplete) OnComplete(false);
			return;
		}
		ToPNGTask.EnterProgressFrame(1);

		FString InputFile = InputFiles[i];
		FString PNGFile = FPaths::Combine(OutputDir, FPaths::GetBaseFilename(InputFile) + ".png");
		OutputFiles.Add(PNGFile);

		if (bScaleAltitude)
		{
			if (!GDALInterface::ConvertToPNG(InputFile, PNGFile, MinAltitude - 100, MaxAltitude + 100))
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