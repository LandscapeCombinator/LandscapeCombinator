// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMResolution.h"
#include "LandscapeCombinator/Directories.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"

#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/Concurrency.h"

#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"


void HMResolution::Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputEPSG = InputEPSG;
	FString ResolutionFolder = FPaths::Combine(Directories::LandscapeCombinatorDir(), LandscapeLabel + "-Resolution" + FString::FromInt(PrecisionPercent));

	if (!IPlatformFile::GetPlatformPhysical().DeleteDirectoryRecursively(*ResolutionFolder) || !IPlatformFile::GetPlatformPhysical().CreateDirectory(*ResolutionFolder))
	{
		Directories::CouldNotInitializeDirectory(ResolutionFolder);
		if (OnComplete) OnComplete(false);
		return;
	}

	FScopedSlowTask ResolutionTask(InputFiles.Num(), LOCTEXT("WarpTask", "GDAL Interface: Scaling Resolution"));
	ResolutionTask.MakeDialog();

	for (int32 i = 0; i < InputFiles.Num(); i++)
	{
		ResolutionTask.EnterProgressFrame(1);
		FString InputFile = InputFiles[i];
		FString OutputFile = FPaths::Combine(ResolutionFolder, FPaths::GetCleanFilename(InputFile));
		OutputFiles.Add(OutputFile);

		if (!GDALInterface::ChangeResolution(InputFile, OutputFile, PrecisionPercent))
		{
			if (OnComplete) OnComplete(false);
			return;
		}
	}
	if (OnComplete) OnComplete(true);
	return;
}


#undef LOCTEXT_NAMESPACE