// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"

#include "HAL/PlatformFile.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h" 

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"


void Directories::CouldNotInitializeDirectory(FString Dir)
{
	ULCReporter::ShowError(
		FText::Format(
			LOCTEXT("DirectoryError", "Could not create or clear directory '{0}'."),
			FText::FromString(Dir)
		)
	);
}

FString Directories::ImageDownloaderDir()
{
	FString Intermediate = FPaths::ConvertRelativePathToFull(FPaths::EngineIntermediateDir());
	FString ImageDownloaderDir = FPaths::Combine(Intermediate, "ImageDownloader");
	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*ImageDownloaderDir))
	{
		CouldNotInitializeDirectory(ImageDownloaderDir);
		return "";
	}

	return ImageDownloaderDir;
}

FString Directories::DownloadDir()
{
	FString ImageDownloaderDir = Directories::ImageDownloaderDir();
	if (ImageDownloaderDir.IsEmpty())
	{
		return "";
	}
	else
	{
		FString DownloadDir = FPaths::Combine(ImageDownloaderDir, "Download");
		if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*DownloadDir))
		{
			CouldNotInitializeDirectory(DownloadDir);
			return "";
		}
		else
		{
			return DownloadDir;
		}
	}
}

#undef LOCTEXT_NAMESPACE
