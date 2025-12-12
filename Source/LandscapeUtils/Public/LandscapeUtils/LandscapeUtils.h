// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Landscape.h"
#include "LandscapeStreamingProxy.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Coordinates/GlobalCoordinates.h"
#include "LCCommon/ActorSelection.h"

using namespace UE::Geometry;

class LANDSCAPEUTILS_API LandscapeUtils
{
public:
	static void MakeDataRelativeTo(int SizeX, int SizeY, uint16* Data, uint16* Base);
	static bool GetLandscapeBounds(ALandscape *Landscape, TArray<ALandscapeStreamingProxy*> LandscapeStreamingProxies, FVector2D &MinMaxX, FVector2D &MinMaxY, FVector2D &MinMaxZ);
	static bool GetLandscapeBounds(ALandscape *Landscape, FVector2D &MinMaxX, FVector2D &MinMaxY, FVector2D &MinMaxZ);
	static bool GetLandscapeMinMaxZ(ALandscape *Landscape, FVector2D &MinMaxZ);
	static TArray<ALandscapeStreamingProxy*> GetLandscapeStreamingProxies(ALandscape *Landscape);
	static bool CustomCollisionQueryParams(AActor* Actor, FCollisionQueryParams &CollisionQueryParams);
	static bool CustomCollisionQueryParams(TArray<AActor*> CollidingActors, FCollisionQueryParams &CollisionQueryParams);
	static bool CustomCollisionQueryParams(const UWorld *World, const FActorSelection &ActorSelection, FCollisionQueryParams &CollisionQueryParams);

	static bool GetZ(UWorld* World, FCollisionQueryParams CollisionQueryParams, double x, double y, double &OutZ, bool bDrawDebugLine = false);
	static bool GetZ(AActor* Actor, double x, double y, double &OutZ, bool bDrawDebugLine = false);

	static bool CreateMeshFromHeightmap(
		int Width, int Height, const TArray<float>& Heightmap, const FVector2D & TopLeftCorner, const FVector2D & BottomRightCorner, double ZScale,
		TArray<FVector> &OutVertices, TArray<FIndex3i> &OutTriangles
	);
	static bool CreateMeshFromHeightmap(int Width, int Height, const TArray<float>& Heightmap, const FVector2D & TopLeftCorner, const FVector2D & BottomRightCorner, double ZScale, FDynamicMesh3 & OutMesh);

	static bool GetLandscapeCRSBounds(ALandscape *Landscape, FVector4d &OutCoordinates);
	static bool GetLandscapeCRSBounds(ALandscape *Landscape, FString ToCRS, FVector4d &OutCoordinates);
	static bool GetLandscapeCRSBounds(ALandscape *Landscape, UGlobalCoordinates *GlobalCoordinates, FString ToCRS, FVector4d &OutCoordinates);
	static bool GetActorCRSBounds(AActor *Actor, FVector4d &OutCoordinates);
	static bool GetActorCRSBounds(AActor *Actor, FString ToCRS, FVector4d &OutCoordinates);
	static bool GetActorCRSBounds(AActor *Actor, UGlobalCoordinates *GlobalCoordinates, FString ToCRS, FVector4d &OutCoordinates);

#if WITH_EDITOR

	static bool SpawnLandscape(
		bool bIgnoreHeightmapData,
		TArray<FString> Heightmaps, FString LandscapeLabel, bool bCreateLandscapeStreamingProxies,
		bool bAutoComponents, bool bDropData,
		int QuadsPerSubsection, int SectionsPerComponent, FIntPoint ComponentCount,
		TObjectPtr<ALandscape> &OutSpawnedLandscape, TArray<TSoftObjectPtr<ALandscapeStreamingProxy>> &OutSpawnedLandscapeStreamingProxies
	);

	static bool CRSToQuadSpace(ALandscape *LandscapeToExtend, ULandscapeInfo *LandscapeInfo, FVector4d &Coordinates, double &OutMinQX, double &OutMaxQX, double &OutMinQY, double &OutMaxQY);
	static bool ExtendLandscape(ALandscape *LandscapeToExtend, FString Heightmap);
	static bool ExtendLandscape(ALandscape *LandscapeToExtend, TArray<FString> Heightmaps);
	static bool UpdateLandscapeComponent(
		ULandscapeComponent* LandscapeComponent, const FString& Heightmap,
		double FMinQX, double FMaxQX, double FMinQY, double FMaxQY
	);

#endif
};
