// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMLocalFolder.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "LCCommon/LCReporter.h"

#include "HAL/FileManagerGeneric.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMLocalFolder::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	FString AllExt = FString("*");
	FString All = FPaths::Combine(Folder, AllExt);
	TArray<FString> FileNames;
	FFileManagerGeneric::Get().FindFiles(FileNames, *All, true, false);

	if (FileNames.Num() == 0)
	{
		ULCReporter::ShowError(FText::Format(
			LOCTEXT("EmptyFolder", "Folder {0} is empty."),
			FText::FromString(Folder)
		));
		if (OnComplete) OnComplete(false);
		return;
	}

	for (auto& FileName : FileNames)
	{
		OutputFiles.Add(FPaths::Combine(Folder, FileName));
	}
	
	if (OnComplete) OnComplete(true);
	return;
}

#undef LOCTEXT_NAMESPACE