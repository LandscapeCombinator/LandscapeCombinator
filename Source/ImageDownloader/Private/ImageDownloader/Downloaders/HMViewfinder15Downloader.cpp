// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMViewfinder15Downloader.h"
#include "LCCommon/LCReporter.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMViewfinder15Downloader::ValidateTiles() const
{
	for (auto &MegaTile : MegaTiles)
	{
		if (MegaTile.Len() != 4 || !MegaTile.StartsWith("15-") || MegaTile[3] < 'A' || MegaTile[3] > 'X')
		{
			ULCReporter::ShowError(
				FText::Format(
					LOCTEXT("HMViewfinder15Downloader::Initialize", "Tile '{0}' is not a valid tile for Viewfinder Panoramas 15."),
					FText::FromString(MegaTile)
				)
			);
			return false;
		}
	}
	return true;
}

#undef LOCTEXT_NAMESPACE