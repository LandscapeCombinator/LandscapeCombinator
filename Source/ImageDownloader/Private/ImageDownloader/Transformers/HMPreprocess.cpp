// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMPreprocess.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "ConsoleHelpers/Console.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "HAL/PlatformFile.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMPreprocess::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	if (!ExternalTool || ExternalTool->Command.IsEmpty())
	{
		LCReporter::ShowError(
			LOCTEXT("HMPreprocess::Fetch", "Please enter a preprocessing command, or disable the preprocessing phase.")
		); 
		return false;
	}
	OutputCRS = InputCRS;

	for (int32 i = 0; i < InputFiles.Num(); i++)
	{
		FString InputFile = InputFiles[i];
		FString Extension = ExternalTool->bChangeExtension ? ExternalTool->NewExtension : FPaths::GetExtension(InputFile);
		FString PreprocessedFile = FPaths::Combine(OutputDir, FPaths::GetBaseFilename(InputFile) + "." + Extension);
		OutputFiles.Add(PreprocessedFile);

		if (!ExternalTool->Run(InputFile, PreprocessedFile)) return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE