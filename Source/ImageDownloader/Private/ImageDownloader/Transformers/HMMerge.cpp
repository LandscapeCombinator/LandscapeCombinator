// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMMerge.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"

#include "HAL/PlatformFile.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMMerge::OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputCRS = InputCRS;

	if (InputFiles.Num() == 1)
	{
		OutputFiles.Add(InputFiles[0]);
		if (OnComplete) OnComplete(true);
		return;
	}
	
	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*OutputDir))
	{
		Directories::CouldNotInitializeDirectory(OutputDir);
		if (OnComplete) OnComplete(false);
		return;
	}

	FString MergedFile = FPaths::Combine(OutputDir, Name + ".tif");

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