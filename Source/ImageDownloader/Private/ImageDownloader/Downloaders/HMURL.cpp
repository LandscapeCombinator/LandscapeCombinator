// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMURL.h"
#include "ImageDownloader/Directories.h"
#include "FileDownloader/Download.h"

void HMURL::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	FString FilePath = FPaths::Combine(Directories::DownloadDir(), FileName);
	OutputFiles.Add(FilePath);
	Download::FromURL(URL, FilePath, OnComplete);
}
