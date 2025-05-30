// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/Downloaders/HMViewfinder15Downloader.h"
#include "Misc/MessageDialog.h"
#include "ConcurrencyHelpers/LCReporter.h"

#define LOCTEXT_NAMESPACE "FImageDownloaderModule"

bool HMViewfinder15Downloader::ValidateTiles() const
{
	for (auto &MegaTile : MegaTiles)
	{
		if (MegaTile.Len() != 4 || !MegaTile.StartsWith("15-") || MegaTile[3] < 'A' || MegaTile[3] > 'X')
		{
			LCReporter::ShowError(
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