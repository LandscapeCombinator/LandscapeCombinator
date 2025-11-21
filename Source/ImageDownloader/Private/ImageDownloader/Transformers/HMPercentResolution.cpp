// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMPercentResolution.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"

#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/Concurrency.h"

#include "HAL/PlatformFile.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"


bool HMPercentResolution::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	OutputCRS = InputCRS;

	for (int32 i = 0; i < InputFiles.Num(); i++)
	{
		FString InputFile = InputFiles[i];
		FString OutputFile = FPaths::Combine(OutputDir, FPaths::GetCleanFilename(InputFile));
		OutputFiles.Add(OutputFile);

		if (!GDALInterface::ChangeResolution(InputFile, OutputFile, PrecisionPercent)) return false;
	}
	return true;
}


#undef LOCTEXT_NAMESPACE