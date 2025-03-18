// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMViewfinderDownloader.h"

class HMViewfinder15Downloader : public HMViewfinderDownloader
{
	bool ValidateTiles() const override;
};
