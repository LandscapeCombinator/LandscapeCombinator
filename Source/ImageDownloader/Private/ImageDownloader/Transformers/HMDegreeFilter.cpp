// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMDegreeFilter.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

void HMDegreeFilter::Fetch(FString InputCRS, TArray<FString> InputFiles, TFunction<void(bool)> OnComplete)
{
	OutputCRS = InputCRS;
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
		ULCReporter::ShowError(
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