// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMResolution.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"

#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/Concurrency.h"

#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"


void HMResolution::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputCRS = InputCRS;
	FString ResolutionFolder = FPaths::Combine(Directories::ImageDownloaderDir(), Name + "-Resolution" + FString::FromInt(PrecisionPercent));

	if (!IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*ResolutionFolder) || !IPlatformFile::GetPlatformPhysical().CreateDirectory(*ResolutionFolder))
	{
		Directories::CouldNotInitializeDirectory(ResolutionFolder);
		if (OnComplete) OnComplete(false);
		return;
	}

	FScopedSlowTask ResolutionTask(InputFiles.Num(), LOCTEXT("WarpTask", "GDAL Interface: Scaling Resolution"));
	ResolutionTask.MakeDialog();

	for (int32 i = 0; i < InputFiles.Num(); i++)
	{
		ResolutionTask.EnterProgressFrame(1);
		FString InputFile = InputFiles[i];
		FString OutputFile = FPaths::Combine(ResolutionFolder, FPaths::GetCleanFilename(InputFile));
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