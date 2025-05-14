// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Transformers/HMDegreeFilter.h"
#include "Misc/MessageDialog.h"
#include "ConcurrencyHelpers/LCReporter.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMDegreeFilter::OnFetch(FString InputCRS, TArray<FString> InputFiles)
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
		LCReporter::ShowError(
			LOCTEXT("HMDegreeFilter", "No files left after Viewfinder Panoramas filtering. Check your MinLong, MaxLong, MinLat, MaxLat values.")
		);
		return false;
	}
	else return true;
}

#undef LOCTEXT_NAMESPACE