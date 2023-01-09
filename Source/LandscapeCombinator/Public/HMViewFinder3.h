#pragma once

#include "HMViewFinder1or3.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class HMViewFinder3 : public HMViewFinder1or3
{

public:
	HMViewFinder3(FString LandscapeName0, const FText &KindText0, FString Descr0, int Precision0) :
		HMViewFinder1or3(LandscapeName0, KindText0, Descr0, Precision0) {

		TileWidth = 1201;
		TileHeight = 1201;
		BaseURL = "http://viewfinderpanoramas.org/dem3/";
	}
};

#undef LOCTEXT_NAMESPACE
