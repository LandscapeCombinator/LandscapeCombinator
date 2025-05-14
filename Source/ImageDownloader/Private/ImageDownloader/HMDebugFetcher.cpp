// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/HMDebugFetcher.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Async/Async.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMDebugFetcher::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	if (!Fetcher)
	{
		return false;
	}

	if (!bAllowEmpty && InputFiles.IsEmpty())
	{
		UE_LOG(LogImageDownloader, Error, TEXT("No input files when entering Phase %s"), *Name);
		return false;
	}

	UE_LOG(LogImageDownloader, Log, TEXT("Running Phase %s on files:\n%s"), *Name, *FString::Join(InputFiles, TEXT("\n")));
	UE_LOG(LogImageDownloader, Log, TEXT("InputCRS: %s"), *InputCRS);

	if (Fetcher->Fetch(InputCRS, InputFiles))
	{
		if (!this)
		{
			LCReporter::ShowError(LOCTEXT("BadThisPointer", "Internal Error: Bad `this` pointer"));
			return false;
		}
		if (!Fetcher)
		{
			LCReporter::ShowError(LOCTEXT("BadFetcherPointer", "Internal Error: Bad `Fetcher` pointer"));
			return false;
		}

		OutputFiles = Fetcher->OutputFiles;
		OutputCRS = Fetcher->OutputCRS;	

		UE_LOG(LogImageDownloader, Log, TEXT("Finished running Phase %s, and got files:\n%s"), *Name, *FString::Join(OutputFiles, TEXT("\n")));
		UE_LOG(LogImageDownloader, Log, TEXT("OutputCRS: %s"), *OutputCRS);

		return true;
	}
	else
	{
		UE_LOG(LogImageDownloader, Error, TEXT("Phase %s failed."), *Name);
		LCReporter::ShowError(
			FText::Format(
				LOCTEXT("HMDebugFetcher::Fetch", "Image Downloader Error: There was an error during Phase {0}."),
				FText::FromString(Name)
			)
		);
		return false;
	}
}

#undef LOCTEXT_NAMESPACE
