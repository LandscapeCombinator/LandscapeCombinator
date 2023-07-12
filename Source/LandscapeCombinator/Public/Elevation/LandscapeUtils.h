// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Landscape.h"
#include "LandscapeStreamingProxy.h"

namespace LandscapeUtils
{
	ALandscape* SpawnLandscape(TArray<FString> Heightmaps, FString LandscapeLabel);
	bool GetLandscapeBounds(ALandscape *Landscape, TArray<ALandscapeStreamingProxy*> LandscapeStreamingProxies, FVector2D &MinMaxX, FVector2D &MinMaxY, FVector2D &MinMaxZ);
	TArray<ALandscapeStreamingProxy*> GetLandscapeStreamingProxies(ALandscape *Landscape);
}
