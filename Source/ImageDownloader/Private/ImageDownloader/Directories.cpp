// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "LCCommon/LCSettings.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "HAL/PlatformFile.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h" 

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"


void Directories::CouldNotInitializeDirectory(FString Dir)
{
	LCReporter::ShowError(
		FText::Format(
			LOCTEXT("DirectoryError", "Could not create or clear directory '{0}'."),
			FText::FromString(Dir)
		)
	);
}

FString Directories::ImageDownloaderDir()
{
	FString Base = GetDefault<ULCSettings>()->TemporaryFolder;
	if (Base.IsEmpty()) Base = FPaths::ConvertRelativePathToFull(FPaths::EngineIntermediateDir());
	else if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*Base))
	{
		CouldNotInitializeDirectory(Base);
		return "";
	}

	FString ImageDownloaderDir = FPaths::Combine(Base, "ImageDownloader");
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
