// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMDebugFetcher.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"

#include "Async/Async.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void HMDebugFetcher::Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	if (!bAllowEmpty && InputFiles.IsEmpty())
	{
		UE_LOG(LogLandscapeCombinator, Error, TEXT("No input files when entering Phase %s"), *Name);
		if (OnComplete) OnComplete(false);
		return;
	}

	UE_LOG(LogLandscapeCombinator, Log, TEXT("Running Phase %s on files:\n%s"), *Name, *FString::Join(InputFiles, TEXT("\n")));
	UE_LOG(LogLandscapeCombinator, Log, TEXT("InputEPSG: %d"), InputEPSG);

	// Always reenter game thread to display progress to the user
	AsyncTask(ENamedThreads::GameThread, [this, InputEPSG, InputFiles, OnComplete]()
	{
		Fetcher->Fetch(InputEPSG, InputFiles, [this, OnComplete](bool bSuccess)
		{
			if (bSuccess)
			{
				OutputFiles = Fetcher->OutputFiles;
				OutputEPSG = Fetcher->OutputEPSG;
				UE_LOG(LogLandscapeCombinator, Log, TEXT("Finished running Phase %s, and got files:\n%s"), *Name, *FString::Join(OutputFiles, TEXT("\n")));
				UE_LOG(LogLandscapeCombinator, Log, TEXT("OutputEPSG: %d"), OutputEPSG);
				if (OnComplete) OnComplete(true);
			}
			else
			{
				UE_LOG(LogLandscapeCombinator, Error, TEXT("Phase %s failed."), *Name);
				FMessageDialog::Open(EAppMsgType::Ok,
					FText::Format(
						LOCTEXT("HMDebugFetcher::Fetch", "Landscape Combinator Error: There was an error during Phase {0}."),
						FText::FromString(Name)
					)
				);
				if (OnComplete) OnComplete(false);
			}
		});
	});
}

#undef LOCTEXT_NAMESPACE
