// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMConvert.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"

#include "HAL/PlatformFile.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMConvert::OnFetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputCRS = InputCRS;

	if (InputFiles[0].EndsWith(NewExtension))
	{
		UE_LOG(LogImageDownloader, Log, TEXT("Skipping conversion phase as input files are already in the correct format (%s)"), *NewExtension);

		OutputFiles.Append(InputFiles);
		if (OnComplete) OnComplete(true);
		return;
	}

	FVector2D Altitudes;
	if (!GDALInterface::GetMinMax(Altitudes, InputFiles))
	{
		if (OnComplete) OnComplete(false);
		return;
	}

	double MinAltitude = Altitudes[0];
	double MaxAltitude = Altitudes[1];

	FScopedSlowTask ConvertTask(
		InputFiles.Num(),
		FText::Format(
			LOCTEXT("ConvertTask", "GDAL Interface: Translating Files to {0}"),
			FText::FromString(NewExtension)
		)
	);
	ConvertTask.MakeDialog(true);

	for (int32 i = 0; i < InputFiles.Num(); i++)
	{
		if (ConvertTask.ShouldCancel())
		{
			if (OnComplete) OnComplete(false);
			return;
		}

		ConvertTask.EnterProgressFrame(1);

		FString InputFile = InputFiles[i];
		FString ConvertedFile = FPaths::Combine(OutputDir, FPaths::GetBaseFilename(InputFile) + "." + NewExtension);
		OutputFiles.Add(ConvertedFile);

		if (!GDALInterface::Translate(InputFile, ConvertedFile, TArray<FString>()))
		{
			if (OnComplete) OnComplete(false);
			return;
		}
	}

	if (OnComplete) OnComplete(true);
	return;
}

#undef LOCTEXT_NAMESPACE