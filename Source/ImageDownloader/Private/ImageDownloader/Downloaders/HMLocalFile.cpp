// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMLocalFile.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "LCReporter/LCReporter.h"

#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMLocalFile::OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	if (FPaths::FileExists(File))
	{
		OutputFiles.Add(File);
		if (OnComplete) OnComplete(true);
		return;
	}
	else
	{
		ULCReporter::ShowError(FText::Format(
			LOCTEXT("HMLocalFile::Fetch", "Image Downloader Error: File {0} does not exist."),
			FText::FromString(File)
		));
		if (OnComplete) OnComplete(false);
		return;
	}
}

#undef LOCTEXT_NAMESPACE