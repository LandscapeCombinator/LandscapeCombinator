// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMReproject.h"
#include "LandscapeCombinator/Directories.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "GDALInterface/GDALInterface.h"

#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void HMReproject::Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	if (OutputEPSG == InputEPSG)
	{
		// no reprojection needed in this case
		OutputFiles.Append(InputFiles);
		if (OnComplete) OnComplete(true);
		return;
	}

	FString ReprojectedFolder = FPaths::Combine(
		Directories::LandscapeCombinatorDir(),
		LandscapeLabel + "-" + FString::FromInt(OutputEPSG)
	);
	
	if (!IPlatformFile::GetPlatformPhysical().CreateDirectory(*ReprojectedFolder))
	{
		Directories::CouldNotInitializeDirectory(ReprojectedFolder);
		if (OnComplete) OnComplete(false);
		return;
	}

	FString ReprojectedFile = FPaths::Combine(
		ReprojectedFolder,
		LandscapeLabel + ".tif"
	);

	UE_LOG(LogLandscapeCombinator, Log, TEXT("Starting reprojection of heightmaps using EPSG %d to output %s"), OutputEPSG, *ReprojectedFile);

	FScopedSlowTask ReprojectTask(1, LOCTEXT("WarpTask", "GDAL Interface: Reprojecting File"));
	ReprojectTask.MakeDialog();

	if (GDALInterface::Warp(InputFiles, ReprojectedFile, InputEPSG, OutputEPSG, 0))
	{
		OutputFiles.Add(ReprojectedFile);
		if (OnComplete) OnComplete(true);
	}
	else
	{
		if (OnComplete) OnComplete(false);
	}
}

#undef LOCTEXT_NAMESPACE