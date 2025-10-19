// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMToPNG.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"


#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMToPNG::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	OutputCRS = InputCRS;

	FVector2D Altitudes;
	if (bScaleAltitude && !GDALInterface::GetMinMax(Altitudes, InputFiles))
	{
		return false;
	}

	double MinAltitude = Altitudes[0];
	double MaxAltitude = Altitudes[1];

	int NumFilesToConvert = bConvertOnlyFirst ? 1 : InputFiles.Num();
	for (int32 i = 0; i < NumFilesToConvert; i++)
	{
		FString InputFile = InputFiles[i];
		FString PNGFile = FPaths::Combine(OutputDir, FPaths::GetBaseFilename(InputFile) + ".png");
		OutputFiles.Add(PNGFile);

		if (bScaleAltitude)
		{
			if (!GDALInterface::ConvertToPNG(InputFile, PNGFile, MinAltitude - 100, MaxAltitude + 100))
			{
				return false;
			}
		}
		else
		{
			if (!GDALInterface::ConvertToPNG(InputFile, PNGFile, 0, 255))
			{
				return false;
			}
		}
	}

	for (int32 i = NumFilesToConvert; i < InputFiles.Num(); i++)
	{
		OutputFiles.Add(InputFiles[i]);
	}
	
	return true;
}

#undef LOCTEXT_NAMESPACE