// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMListDownloader.h"
#include "ImageDownloader/Directories.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "FileDownloader/Download.h"
#include "GDALInterfaceModule.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Interfaces/IPluginManager.h"
#include "HAL/FileManagerGeneric.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMListDownloader::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	FString AbsoluteListOfLinks = FGDALInterfaceModule::PluginDir / ListOfLinks;
	FString FileContent = "";
	if (!FFileHelper::LoadFileToString(FileContent, *AbsoluteListOfLinks) && !FFileHelper::LoadFileToString(FileContent, *ListOfLinks))
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("HMListDownloader::Fetch", "Could not read file {0}."),
				FText::FromString(ListOfLinks)
			)
		); 
		return false;
	}

	TArray<FString> Lines0, Lines;
	FileContent.ParseIntoArray(Lines0, TEXT("\n"), true);
	if (Lines0.Num() == 0)
	{
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("HMListDownloader::Fetch", "File {0} does not contain any link."),
				FText::FromString(ListOfLinks)
			)
		); 
		return false;
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
			LCReporter::ShowError(
				FText::Format(
					LOCTEXT("HMListDownloader::Fetch", "Line {0} is not a valid link."),
					FText::FromString(ListOfLinks)
				)
			); 
			return false;
		}

		FString FileName = Elements[NumElements - 1];
		FString CleanFileName = FPaths::GetCleanFilename(FileName);
		
		FString OutputFile = FPaths::Combine(DownloadDir, FPaths::GetCleanFilename(FileName));
		OutputFiles.Add(OutputFile);
	}

	return Concurrency::RunManyAndWait(
		true,
		Lines.Num(),
		[this, Lines](int i)
		{
			return Download::SynchronousFromURL(Lines[i], OutputFiles[i], bIsUserInitiated);
		}
	);
}

#undef LOCTEXT_NAMESPACE