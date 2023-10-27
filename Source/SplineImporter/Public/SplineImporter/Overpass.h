// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SPLINEIMPORTER_API Overpass
{
public:
	static FString QueryFromShortQuery(double South, double West, double North, double East, FString ShortQuery);
};
