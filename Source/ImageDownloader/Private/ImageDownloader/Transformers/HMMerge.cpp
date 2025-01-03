// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMMerge.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"

#include "HAL/PlatformFile.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMMerge::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputCRS = InputCRS;

	if (InputFiles.Num() == 1)
	{
		OutputFiles.Add(InputFiles[0]);
		if (OnComplete) OnComplete(true);
		return;
	}

	FString MergeFolder = FPaths::Combine(
		Directories::ImageDownloaderDir(),
		Name + "-Merge"
	);
	
	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*MergeFolder))
	{
		Directories::CouldNotInitializeDirectory(MergeFolder);
		if (OnComplete) OnComplete(false);
		return;
	}

	FString MergedFile = FPaths::Combine(MergeFolder, Name + ".tif");

	FScopedSlowTask *MergeTask = new FScopedSlowTask(1, LOCTEXT("MergeTask", "GDAL Interface: Merging Files"));
	MergeTask->MakeDialog();
	bool bMerged = GDALInterface::Merge(InputFiles, MergedFile);
	MergeTask->Destroy();
	if (bMerged)
	{
		OutputFiles.Add(MergedFile);
		if (OnComplete) OnComplete(true);
	}
	else
	{
		if (OnComplete) OnComplete(false);
	}
}

#undef LOCTEXT_NAMESPACE