// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMLocalFolder.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "HAL/FileManagerGeneric.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMLocalFolder::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	FString AllExt = FString("*");
	FString All = FPaths::Combine(Folder, AllExt);
	TArray<FString> FileNames;
	FFileManagerGeneric::Get().FindFiles(FileNames, *All, true, false);

	if (FileNames.Num() == 0)
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("EmptyFolder", "Folder {0} is empty."),
			FText::FromString(Folder)
		));
		return false;
	}

	for (auto& FileName : FileNames)
	{
		OutputFiles.Add(FPaths::Combine(Folder, FileName));
	}
	
	return true;
}

#undef LOCTEXT_NAMESPACE