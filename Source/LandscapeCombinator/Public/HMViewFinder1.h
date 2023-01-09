#pragma once

#include "HMViewFinder1or3.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMViewFinder1 : public HMViewFinder1or3
{

public:
	HMViewFinder1(FString LandscapeName0, const FText &KindText0, FString Descr0, int Precision0) :
		HMViewFinder1or3(LandscapeName0, KindText0, Descr0, Precision0) {

		TileWidth = 3601;
		TileHeight = 3601;
		BaseURL = "http://viewfinderpanoramas.org/dem1/";
	}
};

#undef LOCTEXT_NAMESPACE
