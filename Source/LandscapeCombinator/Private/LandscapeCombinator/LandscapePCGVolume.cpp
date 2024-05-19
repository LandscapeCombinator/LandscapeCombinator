// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapePCGVolume.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void ALandscapePCGVolume::SetPositionAndBounds()
{
	ALandscape *Landscape = Cast<ALandscape>(LandscapeSelection.GetActor(GetWorld()));
	if (!IsValid(Landscape)) return;

	FVector2D MinMaxX, MinMaxY, MinMaxZ;
	if (!LandscapeUtils::GetLandscapeBounds(Landscape, MinMaxX, MinMaxY, MinMaxZ)) return;

	Position = FVector((MinMaxX[0] + MinMaxX[1]) / 2, (MinMaxY[0] + MinMaxY[1]) / 2, 0);
	Bounds = FVector((MinMaxX[1] - MinMaxX[0]) / 2, (MinMaxY[1] - MinMaxY[0]) / 2, 10000000);
	SetActorLocation(Position);
	SetActorScale3D(Bounds / 100);
}

#undef LOCTEXT_NAMESPACE
