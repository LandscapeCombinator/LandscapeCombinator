// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "HMViewFinder1or3.h"
#include "Utils/Logging.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMViewFinder1 : public HMViewFinder1or3
{

public:
	HMViewFinder1(FString LandscapeLabel0, const FText &KindText0, FString Descr0, int Precision0) :
		HMViewFinder1or3(LandscapeLabel0, KindText0, Descr0, Precision0)
	{
		TileWidth = 3601;
		TileHeight = 3601;
		BaseURL = "http://viewfinderpanoramas.org/dem1/";
	}
};

#undef LOCTEXT_NAMESPACE
