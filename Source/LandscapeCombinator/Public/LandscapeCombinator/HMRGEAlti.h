// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMURL.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMRGEALTI
{
public:
	static HMURL* RGEALTI(
		FString LandscapeLabel, FString URL,
		int EPSG,
		double MinLong, double MaxLong,
		double MinLat, double MaxLat,
		bool bOverrideWidthAndHeight, int Width, int Height
	);
};

#undef LOCTEXT_NAMESPACE
