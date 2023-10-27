// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMConvert.h"
#include "LandscapeCombinator/Directories.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "GDALInterface/GDALInterface.h"

#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void HMConvert::Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputEPSG = InputEPSG;
	FString ConvertFolder = FPaths::Combine(Directories::LandscapeCombinatorDir(), LandscapeLabel + "-Convert");

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