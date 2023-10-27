// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMLocalFolder.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"

#include "HAL/FileManagerGeneric.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void HMLocalFolder::Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	FString AllExt = FString("*");
	FString All = FPaths::Combine(Folder, AllExt);
	TArray<FString> FileNames;
	FFileManagerGeneric::Get().FindFiles(FileNames, *All, true, false);

	if (FileNames.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
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