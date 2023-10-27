// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMFetcher.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "ConcurrencyHelpers/Concurrency.h"

#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

HMFetcher* HMFetcher::AndThen(HMFetcher* OtherFetcher)
{
	return new HMAndThenFetcher(this, OtherFetcher);
}

HMFetcher* HMFetcher::AndRun(TFunction<void(HMFetcher*)> Lambda)
{
	return new HMAndRunFetcher(this, Lambda);
}

void HMAndThenFetcher::Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	Fetcher1->Fetch(InputEPSG, InputFiles, [this, OnComplete](bool bSuccess1)
	{
		if (bSuccess1)
		{
			Fetcher2->Fetch(Fetcher1->OutputEPSG, Fetcher1->OutputFiles, [this, OnComplete](bool bSuccess2)
			{
				if (bSuccess2)
				{
					OutputFiles = Fetcher2->OutputFiles;
					if (Fetcher2->OutputEPSG > 0)
					{
						OutputEPSG = Fetcher2->OutputEPSG;
					}
					else
					{
						OutputEPSG = Fetcher1->OutputEPSG;
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

void HMAndRunFetcher::Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	Fetcher->Fetch(InputEPSG, InputFiles, [this, OnComplete](bool bSuccess)
	{
		if (bSuccess)
		{
			OutputFiles = Fetcher->OutputFiles;
			OutputEPSG = Fetcher->OutputEPSG;
			Lambda(this);
			if (OnComplete) OnComplete(true);
		}
		else
		{
			if (OnComplete) OnComplete(false);
		}
	});
}

#undef LOCTEXT_NAMESPACE
