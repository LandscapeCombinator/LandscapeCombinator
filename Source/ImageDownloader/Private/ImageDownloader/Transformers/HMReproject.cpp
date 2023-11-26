// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMReproject.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"

#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMReproject::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	if (OutputCRS == InputCRS)
	{
		UE_LOG(LogImageDownloader, Log, TEXT("Skipping Reprojection phase as the input files are already in the correct CRS: %s"), *InputCRS);
		OutputFiles.Append(InputFiles);
		if (OnComplete) OnComplete(true);
		return;
	}

	FString ReprojectedFolder = FPaths::Combine(
		Directories::ImageDownloaderDir(),
		Name + "-" + OutputCRS.Replace(TEXT(":"), TEXT("_"))
	);
	
	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*ReprojectedFolder))
	{
		Directories::CouldNotInitializeDirectory(ReprojectedFolder);
		if (OnComplete) OnComplete(false);
		return;
	}

	FString ReprojectedFile = FPaths::Combine(
		ReprojectedFolder,
		Name + ".tif"
	);

	UE_LOG(LogImageDownloader, Log, TEXT("Starting reprojection of heightmaps using %s to output %s"), *OutputCRS, *ReprojectedFile);

	FScopedSlowTask *ReprojectTask = new FScopedSlowTask(1, LOCTEXT("WarpTask", "GDAL Interface: Reprojecting File"));
	ReprojectTask->MakeDialog();

	if (GDALInterface::Warp(InputFiles, ReprojectedFile, InputCRS, OutputCRS, 0))
	{
		ReprojectTask->Destroy();
		OutputFiles.Add(ReprojectedFile);
		if (OnComplete) OnComplete(true);
	}
	else
	{
		if (OnComplete) OnComplete(false);
	}
}

#undef LOCTEXT_NAMESPACE