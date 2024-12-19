// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMReadCRS.h"
#include "ImageDownloader/LogImageDownloader.h"

#include "GDALInterface/GDALInterface.h"
#include "Misc/MessageDialog.h"

#include "Misc/CString.h" 

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMReadCRS::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OGRSpatialReference InRs;
	if (!GDALInterface::SetCRSFromFile(InRs, InputFiles[0]))
	{
		ULCReporter::ShowError(
			FText::Format(
				LOCTEXT("HMSetEPSG::Fetch", "Could not read Spatial Reference from file {0}. Please make sure that it is correctly georeferenced."),
				FText::FromString(InputFiles[0])
			)
		); 
		if (OnComplete) OnComplete(false);
		return;
	}

	OGRErr Err = InRs.AutoIdentifyEPSG();
	FString Name = FString(InRs.GetAuthorityName(nullptr));
	int EPSG = FCString::Atoi(*FString(InRs.GetAuthorityCode(nullptr)));
	OutputCRS = "EPSG:" + FString::FromInt(EPSG);

	if (Err != OGRERR_NONE || Name != "EPSG" || EPSG == 0)
	{
		ULCReporter::ShowError(
			FText::Format(
				LOCTEXT("HMSetEPSG::Fetch", "Could not read EPSG code from file {0} (Error: {1}). Authority name and code are {2}:{3}"),
				FText::FromString(InputFiles[0]),
				FText::AsNumber(Err, &FNumberFormattingOptions::DefaultNoGrouping()),
				FText::FromString(Name),
				FText::AsNumber(EPSG, &FNumberFormattingOptions::DefaultNoGrouping())
			)
		); 
		if (OnComplete) OnComplete(false);
		return;
	}

	OutputFiles.Append(InputFiles);
	if (OnComplete) OnComplete(true);
}

#undef LOCTEXT_NAMESPACE