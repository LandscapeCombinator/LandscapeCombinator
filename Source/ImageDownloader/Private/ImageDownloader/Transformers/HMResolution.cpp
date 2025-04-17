// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMResolution.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"

#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/Concurrency.h"

#include "HAL/PlatformFile.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"


void HMResolution::OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputCRS = InputCRS;

	FScopedSlowTask ResolutionTask(InputFiles.Num(), LOCTEXT("WarpTask", "GDAL Interface: Scaling Resolution"));
	ResolutionTask.MakeDialog();

	for (int32 i = 0; i < InputFiles.Num(); i++)
	{
		ResolutionTask.EnterProgressFrame(1);
		FString InputFile = InputFiles[i];
		FString OutputFile = FPaths::Combine(OutputDir, FPaths::GetCleanFilename(InputFile));
		OutputFiles.Add(OutputFile);

		if (!GDALInterface::ChangeResolution(InputFile, OutputFile, PrecisionPercent))
		{
			if (OnComplete) OnComplete(false);
			return;
		}
	}
	if (OnComplete) OnComplete(true);
	return;
}


#undef LOCTEXT_NAMESPACE