// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMReproject.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"

#include "HAL/PlatformFile.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMReproject::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	if (OutputCRS == InputCRS)
	{
		UE_LOG(LogImageDownloader, Log, TEXT("Skipping Reprojection phase as the input files are already in the correct CRS: %s"), *InputCRS);
		OutputFiles.Append(InputFiles);
		return true;
	}

	FString ReprojectedFile = FPaths::Combine(OutputDir, Name + ".tif");
	if (GDALInterface::Warp(InputFiles, ReprojectedFile, InputCRS, OutputCRS, true, 0))
	{	
		OutputFiles.Add(ReprojectedFile);
		return true;
	}
	else
	{
		return false;
	}
}

#undef LOCTEXT_NAMESPACE