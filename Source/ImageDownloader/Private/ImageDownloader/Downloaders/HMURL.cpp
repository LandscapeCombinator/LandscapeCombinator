// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMURL.h"
#include "ImageDownloader/Directories.h"
#include "FileDownloader/Download.h"

#include "Misc/Paths.h"

bool HMURL::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	FString FilePath = FPaths::Combine(DownloadDir, FileName);
	OutputFiles.Add(FilePath);
	return Download::SynchronousFromURL(URL, FilePath, bIsUserInitiated);
}
