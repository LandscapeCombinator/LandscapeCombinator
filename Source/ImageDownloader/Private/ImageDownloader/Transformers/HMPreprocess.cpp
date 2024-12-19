// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMPreprocess.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "ConsoleHelpers/Console.h"

#include "HAL/PlatformFile.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMPreprocess::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
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

	FString Preprocess = FPaths::Combine(Directories::ImageDownloaderDir(), Name + "-Preprocess");

	if (!IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*Preprocess) || !IPlatformFile::GetPlatformPhysical().CreateDirectory(*Preprocess))
	{
		Directories::CouldNotInitializeDirectory(Preprocess);
		if (OnComplete) OnComplete(false);
		return;
	}

	FScopedSlowTask PreprocessTask(InputFiles.Num(), FText::Format(LOCTEXT("PreprocessTask", "Preprocessing Files using Command {0}"), FText::FromString(ExternalTool->Command)));
	PreprocessTask.MakeDialog();

	for (int32 i = 0; i < InputFiles.Num(); i++)
	{
		PreprocessTask.EnterProgressFrame(1);

		FString InputFile = InputFiles[i];
		FString Extension = ExternalTool->bChangeExtension ? ExternalTool->NewExtension : FPaths::GetExtension(InputFile);
		FString PreprocessedFile = FPaths::Combine(Preprocess, FPaths::GetBaseFilename(InputFile) + "." + Extension);
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