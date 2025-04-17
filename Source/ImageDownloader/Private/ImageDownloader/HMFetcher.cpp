// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/HMFetcher.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "LCReporter/LCReporter.h"

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

void HMAndThenFetcher::OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	if (!Fetcher1 || !Fetcher2)
	{
		if (OnComplete) OnComplete(false);
		return;
	}

	Fetcher1->Fetch(InputCRS, InputFiles, [this, OnComplete](bool bSuccess1)
	{
		if (bSuccess1)
		{
			Fetcher2->Fetch(Fetcher1->OutputCRS, Fetcher1->OutputFiles, [this, OnComplete](bool bSuccess2)
			{
				if (bSuccess2)
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
					if (OnComplete) OnComplete(true);
				}
				else
				{
					if (OnComplete) OnComplete(false);
				}

			});
		}
		else
		{
			if (OnComplete) OnComplete(false);
		}
	});
}

void HMAndRunFetcher::OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	Fetcher->Fetch(InputCRS, InputFiles, [this, OnComplete](bool bSuccess)
	{
		if (bSuccess)
		{
			OutputFiles = Fetcher->OutputFiles;
			OutputCRS = Fetcher->OutputCRS;

			bool bSuccessLambda = Lambda(this);

			if (!bSuccessLambda)
			{
				ULCReporter::ShowError(
					LOCTEXT("LambdaFail", "Image Downloader Error: There was an error while running lambda in AndRunFetcher.")
				); 
			}

			if (OnComplete) OnComplete(bSuccessLambda);
		}
		else
		{
			if (OnComplete) OnComplete(false);
		}
	});
}

#undef LOCTEXT_NAMESPACE
