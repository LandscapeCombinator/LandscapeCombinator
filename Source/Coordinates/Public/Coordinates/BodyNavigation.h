// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Coordinates/GlobalCoordinates.h"

struct COORDINATES_API FBodyNavigation
{
	static void Apply(const UGlobalCoordinates& G, const FVector* LastLocation, const FRotator* LastRotation, FVector& Location, FRotator& Rotation);
};
