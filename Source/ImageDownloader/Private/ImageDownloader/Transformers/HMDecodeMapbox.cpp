// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMDecodeMapbox.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "ConsoleHelpers/Console.h"

#include "GDALInterface/GDALInterface.h"

#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMDecodeMapbox::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	//OutputCRS = InputCRS;

	//FString DecodeMapboxFolder = FPaths::Combine(Directories::ImageDownloaderDir(), Name + "-DecodeMapbox");

	//if (!IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*DecodeMapboxFolder) || !IPlatformFile::GetPlatformPhysical().CreateDirectory(*DecodeMapboxFolder))
	//{
	//	Directories::CouldNotInitializeDirectory(DecodeMapboxFolder);
	//	if (OnComplete) OnComplete(false);
	//	return;
	//}

	//for (auto& InputFile : InputFiles)
	//{
	//	FString OutputFile = FPaths::Combine(DecodeMapboxFolder, FPaths::GetBaseFilename(InputFile) + ".tif");

	//	if (!DecodeMapbox(InputFile, OutputFile))
	//	{
	//		if (OnComplete) OnComplete(false);
	//		return;
	//	}

	//	OutputFiles.Add(OutputFile);
	//}

	if (OnComplete) OnComplete(true);
	return;
}

#undef LOCTEXT_NAMESPACE