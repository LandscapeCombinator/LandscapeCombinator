// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "FileDownloaderModule.h"
#include "FileDownloader/Download.h"

#define LOCTEXT_NAMESPACE "FFileDownloaderModule"

IMPLEMENT_MODULE(FFileDownloaderModule, FileDownloader)

void FFileDownloaderModule::StartupModule()
{
	Download::LoadExpectedSizeCache();
}

#undef LOCTEXT_NAMESPACE
