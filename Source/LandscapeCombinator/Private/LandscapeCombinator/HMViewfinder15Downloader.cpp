// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/HMViewfinder15Downloader.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

bool HMViewfinder15Downloader::ValidateTiles() const
{
	for (auto &MegaTile : MegaTiles)
	{
		if (MegaTile.Len() != 4 || !MegaTile.StartsWith("15-") || MegaTile[3] < 'A' || MegaTile[3] > 'X')
		{
			FMessageDialog::Open(EAppMsgType::Ok,
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