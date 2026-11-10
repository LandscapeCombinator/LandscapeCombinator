// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "LCCommon/LCPositionBasedGeneration.h"

void ULCPositionBasedGeneration::ClearGeneratedTilesCache()
{
	GeneratedTiles.Empty();
	PendingTiles.Empty();
}
