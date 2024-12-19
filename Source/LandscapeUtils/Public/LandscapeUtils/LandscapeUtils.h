// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Landscape.h"
#include "LandscapeStreamingProxy.h"
#include "DynamicMesh/DynamicMesh3.h"

using namespace UE::Geometry;

class LANDSCAPEUTILS_API LandscapeUtils
{
public:
	static void MakeDataRelativeTo(int SizeX, int SizeY, uint16* Data, uint16* Base);
	static bool GetLandscapeBounds(ALandscape *Landscape, TArray<ALandscapeStreamingProxy*> LandscapeStreamingProxies, FVector2D &MinMaxX, FVector2D &MinMaxY, FVector2D &MinMaxZ);
	static bool GetLandscapeBounds(ALandscape *Landscape, FVector2D &MinMaxX, FVector2D &MinMaxY, FVector2D &MinMaxZ);
	static bool GetLandscapeMinMaxZ(ALandscape *Landscape, FVector2D &MinMaxZ);
	static TArray<ALandscapeStreamingProxy*> GetLandscapeStreamingProxies(ALandscape *Landscape);
	static FCollisionQueryParams CustomCollisionQueryParams(AActor* Actor);
	static bool GetZ(UWorld* World, FCollisionQueryParams CollisionQueryParams, double x, double y, double &OutZ, bool bDrawDebugLine = false);
	static bool GetZ(AActor* Actor, double x, double y, double &OutZ, bool bDrawDebugLine = false);

	static bool CreateMeshFromHeightmap(
		int Width, int Height, const TArray<float>& Heightmap, const FVector2D & TopLeftCorner, const FVector2D & BottomRightCorner, double ZScale,
		TArray<FVector> &OutVertices, TArray<FIndex3i> &OutTriangles
	);
	static bool CreateMeshFromHeightmap(int Width, int Height, const TArray<float>& Heightmap, const FVector2D & TopLeftCorner, const FVector2D & BottomRightCorner, double ZScale, FDynamicMesh3 & OutMesh);


#if WITH_EDITOR
	static bool SpawnLandscape(
		TArray<FString> Heightmaps, FString LandscapeLabel, bool bCreateLandscapeStreamingProxies,
		bool bAutoComponents, bool bDropData,
		int QuadsPerSubsection, int SectionsPerComponent, FIntPoint ComponentCount,
		ALandscape* &OutSpawnedLandscape, TArray<ALandscapeStreamingProxy*> &OutSpawnedLandscapeStreamingProxies
	);
#endif
};
