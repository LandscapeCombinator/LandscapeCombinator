// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMLocalFile.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMLocalFile::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	if (FPaths::FileExists(File))
	{
		OutputFiles.Add(File);
		return true;
	}
	else
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("HMLocalFile::Fetch", "Image Downloader Error: File {0} does not exist."),
			FText::FromString(File)
		));
		return false;
	}
}

#undef LOCTEXT_NAMESPACE