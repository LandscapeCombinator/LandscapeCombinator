// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMDegreeFilter.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void HMDegreeFilter::Fetch(int InputEPSG, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputEPSG = InputEPSG;
	for (auto& InputFile : InputFiles)
	{
		int X = TileToX(InputFile);
		int Y = - TileToY(InputFile);
		if (MinLong <= X && X <= MaxLong && MinLat <= Y && Y <= MaxLat)
		{
			OutputFiles.Add(InputFile);
		}
	}

	if (OutputFiles.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("HMDegreeFilter", "No files left after Viewfinder Panoramas filtering. Check your MinLong, MaxLong, MinLat, MaxLat values.")
		); 
		if (OnComplete) OnComplete(false);
	}
	else
	{
		if (OnComplete) OnComplete(true);
	}
}

#undef LOCTEXT_NAMESPACE