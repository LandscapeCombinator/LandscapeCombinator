// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMListDownloader.h"
#include "ImageDownloader/Directories.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "FileDownloader/Download.h"
#include "LCReporter/LCReporter.h"
#include "GDALInterfaceModule.h"

#include "Interfaces/IPluginManager.h"
#include "HAL/FileManagerGeneric.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMListDownloader::OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	FString AbsoluteListOfLinks = FGDALInterfaceModule::PluginDir / ListOfLinks;
	FString FileContent = "";
	if (!FFileHelper::LoadFileToString(FileContent, *AbsoluteListOfLinks) && !FFileHelper::LoadFileToString(FileContent, *ListOfLinks))
	{
		ULCReporter::ShowError(
			FText::Format(
				LOCTEXT("HMListDownloader::Fetch", "Could not read file {0}."),
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
		ULCReporter::ShowError(
			FText::Format(
				LOCTEXT("HMListDownloader::Fetch", "File {0} does not contain any link."),
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
			ULCReporter::ShowError(
				FText::Format(
					LOCTEXT("HMListDownloader::Fetch", "Line {0} is not a valid link."),
					FText::FromString(ListOfLinks)
				)
			); 
			if (OnComplete) OnComplete(false);
			return;
		}

		FString FileName = Elements[NumElements - 1];
		FString CleanFileName = FPaths::GetCleanFilename(FileName);
		
		FString OutputFile = FPaths::Combine(DownloadDir, FPaths::GetCleanFilename(FileName));
		OutputFiles.Add(OutputFile);
	}

	Concurrency::RunMany(
		Lines.Num(),
		[this, Lines](int i, TFunction<void (bool)> OnCompleteElement)
		{
			Download::FromURL(Lines[i], OutputFiles[i], bIsUserInitiated, OnCompleteElement);
		},
		OnComplete
	);
}

#undef LOCTEXT_NAMESPACE