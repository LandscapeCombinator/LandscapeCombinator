// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMCrop.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "HAL/PlatformFile.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMCrop::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	OutputCRS = InputCRS;

	if (Coordinates == FVector4d::Zero())
	{
		OutputFiles.Append(InputFiles);
		return true;
	}

	double BoundingSouth = Coordinates[2];
	double BoundingWest = Coordinates[0];
	double BoundingNorth = Coordinates[3];
	double BoundingEast = Coordinates[1];

	for (int32 i = 0; i < InputFiles.Num(); i++)
	{
		FString InputFile = InputFiles[i];
		FString CroppedFile = FPaths::Combine(OutputDir, FPaths::GetBaseFilename(InputFile) + ".tif");
	
		FVector4d FileCoordinates;
		if (!GDALInterface::GetCoordinates(FileCoordinates, InputFile)) return false;

		double South = FMath::Max(FileCoordinates[2], BoundingSouth);
		double West = FMath::Max(FileCoordinates[0], BoundingWest);
		double North = FMath::Min(FileCoordinates[3], BoundingNorth);
		double East = FMath::Min(FileCoordinates[1], BoundingEast);

		OutputFiles.Add(CroppedFile);
	
		TArray<FString> Args;
		Args.Add("-projwin_srs");
		Args.Add(InputCRS);

		Args.Add("-projwin");
		Args.Add(FString::SanitizeFloat(West));
		Args.Add(FString::SanitizeFloat(North));
		Args.Add(FString::SanitizeFloat(East));
		Args.Add(FString::SanitizeFloat(South));

		if (!GDALInterface::Translate(InputFile, CroppedFile, Args)) return false;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE