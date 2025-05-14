// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMMerge.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"

#include "HAL/PlatformFile.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMMerge::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	OutputCRS = InputCRS;

	if (InputFiles.Num() == 1)
	{
		OutputFiles.Add(InputFiles[0]);
		return true;
	}
	
	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*OutputDir))
	{
		Directories::CouldNotInitializeDirectory(OutputDir);
		return false;
	}

	FString MergedFile = FPaths::Combine(OutputDir, Name + ".tif");

	if (GDALInterface::Merge(InputFiles, MergedFile))
	{
		OutputFiles.Add(MergedFile);
		return true;
	}
	else
	{
		return false;
	}
}

#undef LOCTEXT_NAMESPACE