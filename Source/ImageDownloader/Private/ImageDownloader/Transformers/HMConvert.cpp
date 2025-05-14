// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMConvert.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"

#include "HAL/PlatformFile.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMConvert::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	OutputCRS = InputCRS;

	if (InputFiles[0].EndsWith(NewExtension))
	{
		UE_LOG(LogImageDownloader, Log, TEXT("Skipping conversion phase as input files are already in the correct format (%s)"), *NewExtension);

		OutputFiles.Append(InputFiles);
		return true;
	}

	FVector2D Altitudes;
	if (!GDALInterface::GetMinMax(Altitudes, InputFiles)) return false;

	double MinAltitude = Altitudes[0];
	double MaxAltitude = Altitudes[1];

	for (int32 i = 0; i < InputFiles.Num(); i++)
	{
		FString InputFile = InputFiles[i];
		FString ConvertedFile = FPaths::Combine(OutputDir, FPaths::GetBaseFilename(InputFile) + "." + NewExtension);
		OutputFiles.Add(ConvertedFile);

		if (!GDALInterface::Translate(InputFile, ConvertedFile, TArray<FString>())) return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE