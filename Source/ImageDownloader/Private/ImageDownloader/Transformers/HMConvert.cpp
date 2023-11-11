// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMConvert.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"

#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMConvert::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputCRS = InputCRS;
	FString ConvertFolder = FPaths::Combine(Directories::ImageDownloaderDir(), Name + "-Convert");

	if (!IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*ConvertFolder) || !IPlatformFile::GetPlatformPhysical().CreateDirectory(*ConvertFolder))
	{
		Directories::CouldNotInitializeDirectory(ConvertFolder);
		if (OnComplete) OnComplete(false);
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
	ConvertTask.MakeDialog();

	for (int32 i = 0; i < InputFiles.Num(); i++)
	{
		ConvertTask.EnterProgressFrame(1);

		FString InputFile = InputFiles[i];
		FString ConvertedFile = FPaths::Combine(ConvertFolder, FPaths::GetBaseFilename(InputFile) + "." + NewExtension);
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