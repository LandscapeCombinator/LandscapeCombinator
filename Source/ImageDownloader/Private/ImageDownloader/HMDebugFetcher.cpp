// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/HMDebugFetcher.h"
#include "ImageDownloader/LogImageDownloader.h"

#include "Async/Async.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMDebugFetcher::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	if (!Fetcher)
	{
		if (OnComplete) OnComplete(false);
		return;
	}

	if (!bAllowEmpty && InputFiles.IsEmpty())
	{
		UE_LOG(LogImageDownloader, Error, TEXT("No input files when entering Phase %s"), *Name);
		if (OnComplete) OnComplete(false);
		return;
	}

	UE_LOG(LogImageDownloader, Log, TEXT("Running Phase %s on files:\n%s"), *Name, *FString::Join(InputFiles, TEXT("\n")));
	UE_LOG(LogImageDownloader, Log, TEXT("InputCRS: %s"), *InputCRS);

	// Always reenter game thread to display progress to the user
	AsyncTask(ENamedThreads::GameThread, [this, InputCRS, InputFiles, OnComplete]()
	{
		Fetcher->Fetch(InputCRS, InputFiles, [this, OnComplete](bool bSuccess)
		{
			if (bSuccess)
			{
				OutputFiles = Fetcher->OutputFiles;
				OutputCRS = Fetcher->OutputCRS;
				UE_LOG(LogImageDownloader, Log, TEXT("Finished running Phase %s, and got files:\n%s"), *Name, *FString::Join(OutputFiles, TEXT("\n")));
				UE_LOG(LogImageDownloader, Log, TEXT("OutputCRS: %s"), *OutputCRS);
				if (OnComplete) OnComplete(true);
			}
			else
			{
				UE_LOG(LogImageDownloader, Error, TEXT("Phase %s failed."), *Name);
				FMessageDialog::Open(EAppMsgType::Ok,
					FText::Format(
						LOCTEXT("HMDebugFetcher::Fetch", "Image Downloader Error: There was an error during Phase {0}."),
						FText::FromString(Name)
					)
				);
				if (OnComplete) OnComplete(false);
			}
		});
	});
}

#undef LOCTEXT_NAMESPACE
