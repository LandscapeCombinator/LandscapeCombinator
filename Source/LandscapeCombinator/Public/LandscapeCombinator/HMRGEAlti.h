// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMURL.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMRGEALTI
{
public:
	static HMURL* RGEALTI(FString LandscapeLabel, int MinLong, int MaxLong, int MinLat, int MaxLat, bool bOverrideWidthAndHeight, int Width, int Height);
};

#undef LOCTEXT_NAMESPACE
