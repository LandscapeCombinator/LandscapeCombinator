// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMSetEPSG.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"

#include "GDALInterface/GDALInterface.h"

#include "Misc/CString.h" 

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void HMSetEPSG::Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OGRSpatialReference InRs;
	if (!GDALInterface::SetCRSFromFile(InRs, InputFiles[0]))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
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
	OutputEPSG = FCString::Atoi(*FString(InRs.GetAuthorityCode(nullptr)));
	
	if (Err != OGRERR_NONE || Name != "EPSG" || OutputEPSG == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("HMSetEPSG::Fetch", "Could not read EPSG code from file {0} (Error: {1}). Authority name and code are {2}:{3}"),
				FText::FromString(InputFiles[0]),
				FText::AsNumber(Err),
				FText::FromString(Name),
				FText::AsNumber(OutputEPSG)
			)
		); 
		if (OnComplete) OnComplete(false);
		return;
	}

	OutputFiles.Append(InputFiles);
	if (OnComplete) OnComplete(true);
}

#undef LOCTEXT_NAMESPACE