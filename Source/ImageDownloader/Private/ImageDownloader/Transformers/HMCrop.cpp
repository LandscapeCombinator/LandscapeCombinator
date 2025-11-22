// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMCrop.h"
#include "ImageDownloader/Directories.h"
#include "ImageDownloader/LogImageDownloader.h"
#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "Coordinates/GlobalCoordinates.h"

#include "HAL/PlatformFile.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMCrop::OnFetch(FString InputCRS, TArray<FString> InputFiles)
{
	OutputCRS = InputCRS;

	FVector4d Coordinates(0, 0, 0, 0);
	if (bCropFollowingParametersSelection)
	{	
		if (ParametersSelection.ParametersSelectionMethod == EParametersSelectionMethod::FromBoundingActor)
		{
			if (!IsValid(ParametersSelection.ParametersBoundingActor))
			{
				LCReporter::ShowError(
					LOCTEXT("UImageDownloader::CreateFetcher::InvalidBoundingActor", "Invalid Bounding Actor in parameters selection")
				);
				return false;
			}

			if (!LandscapeUtils::GetActorCRSBounds(ParametersSelection.ParametersBoundingActor, InputCRS, Coordinates))
			{
				LCReporter::ShowError(FText::Format(
					LOCTEXT("UImageDownloader::CreateFetcher::NoCoordinates", "Could not compute bounding coordinates of Actor {0}"),
					FText::FromString(CroppingActor->GetActorNameOrLabel())
				));
				return false;
			}
		}
		else if (ParametersSelection.ParametersSelectionMethod == EParametersSelectionMethod::FromEPSG4326Box)
		{
			FVector4d InCoordinates;
			InCoordinates[0] = ParametersSelection.MinLong;
			InCoordinates[1] = ParametersSelection.MaxLong;
			InCoordinates[2] = ParametersSelection.MinLat;
			InCoordinates[3] = ParametersSelection.MaxLat;

			if (!GDALInterface::ConvertCoordinates(InCoordinates, Coordinates, "EPSG:4326", InputCRS)) return false;
		}
		else if (ParametersSelection.ParametersSelectionMethod == EParametersSelectionMethod::FromEPSG4326Coordinates)
		{
			FVector4d InCoordinates = UGlobalCoordinates::GetCoordinatesFromSize(ParametersSelection.Longitude, ParametersSelection.Latitude, ParametersSelection.RealWorldWidth, ParametersSelection.RealWorldHeight);

			if (!GDALInterface::ConvertCoordinates(InCoordinates, Coordinates, "EPSG:4326", InputCRS)) return false;
		}
		else
		{
			LCReporter::ShowError(
				LOCTEXT("UImageDownloader::CreateFetcher::ManualParametersSelection", "Manual parameters selection cannot be used for cropping")
			);
			return false;
		}
	}
	else
	{
		if (!IsValid(CroppingActor))
		{
			LCReporter::ShowError(
				LOCTEXT("UImageDownloader::CreateFetcher::NoActor", "Please select a Cropping Actor to crop the output image.")
			);
			return false;
		}

		if (!LandscapeUtils::GetActorCRSBounds(CroppingActor, InputCRS, Coordinates))
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("UImageDownloader::CreateFetcher::NoCoordinates", "Could not compute bounding coordinates of Actor {0}"),
				FText::FromString(CroppingActor->GetActorNameOrLabel())
			));
			return false;
		}
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