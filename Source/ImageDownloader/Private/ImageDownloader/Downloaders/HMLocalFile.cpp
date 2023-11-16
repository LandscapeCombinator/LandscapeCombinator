// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMLocalFile.h"
#include "ImageDownloader/LogImageDownloader.h"


#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMLocalFile::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	if (FPaths::FileExists(File))
	{
		OutputFiles.Add(File);
		if (OnComplete) OnComplete(true);
		return;
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("HMLocalFile::Fetch", "Image Downloader Error: File {0} does not exist."),
			FText::FromString(File)
		));
		if (OnComplete) OnComplete(false);
		return;
	}
}

#undef LOCTEXT_NAMESPACE