// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/HMFetcher.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

HMFetcher* HMFetcher::AndThen(HMFetcher* OtherFetcher)
{
	return new HMAndThenFetcher(this, OtherFetcher);
}

HMFetcher* HMFetcher::AndRun(TFunction<bool(HMFetcher*)> Lambda)
{
	return new HMAndRunFetcher(this, Lambda);
}

bool HMAndThenFetcher::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	if (!Fetcher1 || !Fetcher2)
	{
		return false;
	}

	if (Fetcher1->Fetch(InputCRS, InputFiles) &&
		Fetcher2->Fetch(Fetcher1->OutputCRS, Fetcher1->OutputFiles))
	{
		OutputFiles = Fetcher2->OutputFiles;
		if (!Fetcher2->OutputCRS.IsEmpty())
		{
			OutputCRS = Fetcher2->OutputCRS;
		}
		else
		{
			OutputCRS = Fetcher1->OutputCRS;
		}
		return true;
	}
	else
	{
		return false;
	}
}

bool HMAndRunFetcher::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	if (Fetcher->Fetch(InputCRS, InputFiles))
	{
		OutputFiles = Fetcher->OutputFiles;
		OutputCRS = Fetcher->OutputCRS;

		if (Lambda(this)) return true;
		else
		{
			LCReporter::ShowError(
				LOCTEXT("LambdaFail", "Image Downloader Error: There was an error while running lambda in AndRunFetcher.")
			);
			return false;
		}
	}
	else
	{
		return false;
	}
}

#undef LOCTEXT_NAMESPACE
