// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMListDownloader.h"
#include "LandscapeCombinator/Directories.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "FileDownloader/Download.h"

#include "HAL/FileManagerGeneric.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void HMListDownloader::Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	FString FileContent = "";
	if (!FFileHelper::LoadFileToString(FileContent, *ListOfLinks))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("InterfaceFromKindError", "Could not read file {0}."),
				FText::FromString(ListOfLinks)
			)
		); 
		if (OnComplete) OnComplete(false);
		return;
	}

	TArray<FString> Lines0, Lines;
	FileContent.ParseIntoArray(Lines0, TEXT("\n"), true);

	if (Lines0.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("InterfaceFromKindError", "File {0} does not contain any link."),
				FText::FromString(ListOfLinks)
			)
		); 
		if (OnComplete) OnComplete(false);
		return;
	}

	for (auto& Line : Lines0)
	{
		Lines.Add(Line.TrimStartAndEnd());
	}

	for (auto& Line : Lines)
	{
		TArray<FString> Elements;
		int NumElements = Line.ParseIntoArray(Elements, TEXT("/"), true);

		if (NumElements == 0)
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("InterfaceFromKindError", "Line {0} is not a valid link."),
					FText::FromString(ListOfLinks)
				)
			); 
			if (OnComplete) OnComplete(false);
			return;
		}

		FString FileName = Elements[NumElements - 1];
		FString CleanFileName = FPaths::GetCleanFilename(FileName);
		
		FString OutputFile = FPaths::Combine(Directories::LandscapeCombinatorDir(), FPaths::GetCleanFilename(FileName));
		OutputFiles.Add(OutputFile);
	}

	Concurrency::RunMany(
		Lines.Num(),
		[this, Lines](int i, TFunction<void (bool)> OnCompleteElement)
		{
			Download::FromURL(Lines[i], OutputFiles[i], OnCompleteElement);
		},
		OnComplete
	);
}

#undef LOCTEXT_NAMESPACE