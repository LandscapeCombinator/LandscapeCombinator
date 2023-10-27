// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMURL.h"
#include "LandscapeCombinator/Directories.h"
#include "FileDownloader/Download.h"

void HMURL::Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	FString FilePath = FPaths::Combine(Directories::DownloadDir(), FileName);
	OutputFiles.Add(FilePath);
	Download::FromURL(URL, FilePath, OnComplete);
}
