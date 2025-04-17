// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMPreprocess.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "ConsoleHelpers/Console.h"
#include "LCReporter/LCReporter.h"

#include "HAL/PlatformFile.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMPreprocess::OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	if (!ExternalTool || ExternalTool->Command.IsEmpty())
	{
		ULCReporter::ShowError(
			LOCTEXT("HMPreprocess::Fetch", "Please enter a preprocessing command, or disable the preprocessing phase.")
		); 
		if (OnComplete) OnComplete(false);
		return;
	}
	OutputCRS = InputCRS;

	FScopedSlowTask PreprocessTask(InputFiles.Num(), FText::Format(LOCTEXT("PreprocessTask", "Preprocessing Files using Command {0}"), FText::FromString(ExternalTool->Command)));
	PreprocessTask.MakeDialog();

	for (int32 i = 0; i < InputFiles.Num(); i++)
	{
		PreprocessTask.EnterProgressFrame(1);

		FString InputFile = InputFiles[i];
		FString Extension = ExternalTool->bChangeExtension ? ExternalTool->NewExtension : FPaths::GetExtension(InputFile);
		FString PreprocessedFile = FPaths::Combine(OutputDir, FPaths::GetBaseFilename(InputFile) + "." + Extension);
		OutputFiles.Add(PreprocessedFile);

		if (!ExternalTool->Run(InputFile, PreprocessedFile))
		{
			if (OnComplete) OnComplete(false);
			return;
		}
	}

	if (OnComplete) OnComplete(true);
	return;
}

#undef LOCTEXT_NAMESPACE