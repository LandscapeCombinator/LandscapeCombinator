// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Landscape.h"
#include "LandscapeStreamingProxy.h"

class LANDSCAPEUTILS_API LandscapeUtils
{
public:
	static ALandscape* SpawnLandscape(TArray<FString> Heightmaps, FString LandscapeLabel, bool bDropData);
	static bool GetLandscapeBounds(ALandscape *Landscape, TArray<ALandscapeStreamingProxy*> LandscapeStreamingProxies, FVector2D &MinMaxX, FVector2D &MinMaxY, FVector2D &MinMaxZ);
	static bool GetLandscapeBounds(ALandscape *Landscape, FVector2D &MinMaxX, FVector2D &MinMaxY, FVector2D &MinMaxZ);
	static bool GetLandscapeMinMaxZ(ALandscape *Landscape, FVector2D &MinMaxZ);
	static TArray<ALandscapeStreamingProxy*> GetLandscapeStreamingProxies(ALandscape *Landscape);
	static ALandscape* GetLandscapeFromLabel(FString LandscapeLabel);
	static FCollisionQueryParams CustomCollisionQueryParams(AActor* Actor);
	static bool GetZ(UWorld* World, FCollisionQueryParams CollisionQueryParams, double x, double y, double &OutZ);
};
