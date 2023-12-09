// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Landscape.h"
#include "LandscapeStreamingProxy.h"

class LANDSCAPEUTILS_API LandscapeUtils
{
public:
	static void MakeDataRelativeTo(int SizeX, int SizeY, uint16* Data, uint16* Base);
	static ALandscape* SpawnLandscape(
		TArray<FString> Heightmaps, FString LandscapeLabel, bool bCreateLandscapeStreamingProxies,
		bool bAutoComponents, bool bDropData,
		int QuadsPerSubsection, int SectionsPerComponent, FIntPoint ComponentCount
	);
	static bool GetLandscapeBounds(ALandscape *Landscape, TArray<ALandscapeStreamingProxy*> LandscapeStreamingProxies, FVector2D &MinMaxX, FVector2D &MinMaxY, FVector2D &MinMaxZ);
	static bool GetLandscapeBounds(ALandscape *Landscape, FVector2D &MinMaxX, FVector2D &MinMaxY, FVector2D &MinMaxZ);
	static bool GetLandscapeMinMaxZ(ALandscape *Landscape, FVector2D &MinMaxZ);
	static TArray<ALandscapeStreamingProxy*> GetLandscapeStreamingProxies(ALandscape *Landscape);
	static ALandscape* GetLandscapeFromLabel(FString LandscapeLabel);
	static FCollisionQueryParams CustomCollisionQueryParams(AActor* Actor);
	static bool GetZ(UWorld* World, FCollisionQueryParams CollisionQueryParams, double x, double y, double &OutZ);
};
