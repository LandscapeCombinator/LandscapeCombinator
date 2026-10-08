// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Coordinates/GlobalCoordinates.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "HAL/ThreadSafeCounter64.h"
#include "HAL/CriticalSection.h"
#include "LandscapeMesh.generated.h"

UENUM(BlueprintType)
enum class EGridSplitDirection : uint8
{
	Forward,
	Backward,
	Checkerboard // diagonal alternates from quad to quad
};

USTRUCT()
struct FLastMeshSettings
{
	GENERATED_BODY()

	// false until the mesh has been regenerated once
	UPROPERTY()
	bool bValid = false;

	UPROPERTY()
	double SplitNormalsAngle = 0.0;

	UPROPERTY()
	EGridSplitDirection SplitDirection = EGridSplitDirection::Checkerboard;

	UPROPERTY()
	double ApronWidth = 0.0;

	UPROPERTY()
	double ApronDepth = 0.0;
};

USTRUCT()
struct FLandscapeMeshHeightmap
{
	GENERATED_BODY()

	UPROPERTY()
	FName Owner;

	UPROPERTY()
	FVector4d SourceCoordinates = FVector4d(0, 0, 0, 0);

	UPROPERTY()
	int Priority = 0;

	UPROPERTY()
	int Width = 0;

	UPROPERTY()
	int Height = 0;

	// Axis-aligned bounding rectangle of the heightmap, in Unreal world space (Left, Right, Bottom, Top).
	UPROPERTY()
	FVector4d Rect = FVector4d(0, 0, 0, 0);

	// The heightmap's points, in Unreal world space, stored row-major (Width x Height).
	UPROPERTY()
	TArray<FVector> Points;
};

USTRUCT()
struct FRegisteredHeightmap
{
	GENERATED_BODY()

	int Priority = 0;

	// Axis-aligned bounding rectangle of the heightmap, in Unreal world space (Left, Right, Bottom, Top).
	FVector4d Rect = FVector4d(0, 0, 0, 0);

	TWeakObjectPtr<class ALandscapeMesh> Mesh;
};

struct FLandscapeMeshGrid;

UCLASS(BlueprintType)
class LANDSCAPECOMBINATOR_API ALandscapeMesh : public AActor
{
	GENERATED_BODY()

public:
	ALandscapeMesh();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "LandscapeMesh")
	void Clear();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "LandscapeMesh")
	TObjectPtr<UDynamicMeshComponent> MeshComponent;

	UPROPERTY()
	TArray<FLandscapeMeshHeightmap> Heightmaps;

	bool AddHeightmap(int Priority, FVector4d Coordinates, UGlobalCoordinates* GlobalCoordinates, FString File, FName OwnerName = NAME_None);
	bool HasHeightmap(FName OwnerName, const FVector4d& SourceCoordinates);
	int32 RemoveHeightmaps(FName OwnerName);

	UFUNCTION(BlueprintCallable, Category = "LandscapeMesh")
	bool RegenerateMesh(double SplitNormalsAngle, EGridSplitDirection SplitDirection, double ApronWidth = 0.0, double ApronDepth = 0.0);

	static void RegisterAndCutLowerPriority(ALandscapeMesh* Mesh, double SplitNormalsAngle, EGridSplitDirection SplitDirection, double ApronWidth = 0.0, double ApronDepth = 0.0);

	static void Unregister(ALandscapeMesh* Mesh, bool bRestoreCropped = true);

	FLastMeshSettings GetLastSettings();

protected:
	virtual void Destroyed() override;
	virtual void BeginPlay() override;
	virtual void PostRegisterAllComponents() override;

	bool CookCollisionFromCurrentMesh();
	bool bHasCookedCollisionThisSession = false;

	FCriticalSection CookLock;
	FCriticalSection HeightmapsLock;
	FThreadSafeCounter64 MeshGenerationCounter;
	UPROPERTY()
	FLastMeshSettings LastSettings;

	static bool IsPointCoveredByHigherOrEqualPriority(const FVector2D& Point, int Priority, ALandscapeMesh* Self);

	static FCriticalSection RegistryLock;
	static TArray<FRegisteredHeightmap> GRegisteredHeightmaps;

	static TArray<FRegisteredHeightmap> MakeRegistryEntries(ALandscapeMesh* Mesh);
	static void CollectMeshesCroppedBy(const TArray<FRegisteredHeightmap>& Entries, ALandscapeMesh* Skip, TArray<TWeakObjectPtr<ALandscapeMesh>>& OutMeshes);
	static void RegenerateWithOwnSettings(const TArray<TWeakObjectPtr<ALandscapeMesh>>& Meshes, const FLastMeshSettings& Fallback, bool bAsync);
	static bool IsCoveredBySiblingHeightmap(const TArray<FLandscapeMeshHeightmap>& Siblings, int32 SelfIndex, const FVector2D& Point);
	void ComputeCoverage(FLandscapeMeshGrid& Grid, int32 HeightmapIndex, const TArray<FLandscapeMeshHeightmap>& Siblings);
	static void BuildGridForHeightmap(UE::Geometry::FDynamicMesh3& Mesh, FLandscapeMeshGrid& Grid, const FTransform& WorldToMesh, EGridSplitDirection SplitDirection);
	static void BuildApronForHeightmap(UE::Geometry::FDynamicMesh3& Mesh, FLandscapeMeshGrid& Grid, const FTransform& WorldToMesh, double ApronWidth, double ApronDepth);
	void FillGapsWithCDT(UE::Geometry::FDynamicMesh3& Mesh, const FTransform& WorldToMesh, const TArray<FLandscapeMeshHeightmap>& AllHeightmaps);
};
