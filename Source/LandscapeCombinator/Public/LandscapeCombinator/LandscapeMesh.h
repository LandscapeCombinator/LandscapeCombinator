// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Coordinates/GlobalCoordinates.h"
#include "Components/DynamicMeshComponent.h"
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
struct FRegisteredHeightmap
{
	GENERATED_BODY()

	int Priority = 0;

	// Axis-aligned bounding rectangle of the heightmap, in Unreal world space (Left, Right, Bottom, Top).
	FVector4d Rect = FVector4d(0, 0, 0, 0);

	TWeakObjectPtr<class ALandscapeMesh> Mesh;
};

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

	// The heightmap's points, in Unreal world space, stored row-major (Width x Height),
	UPROPERTY()
	TArray<FVector> Points;

	UPROPERTY()
	int Width = 0;

	UPROPERTY()
	int Height = 0;

	UPROPERTY()
	int Priority = 0;

	// Axis-aligned bounding rectangle of this heightmap, in Unreal world space (Left, Right, Bottom, Top).
	UPROPERTY()
	FVector4d Rect = FVector4d(0, 0, 0, 0);

	bool AddHeightmap(int Priority, FVector4d Coordinates, UGlobalCoordinates* GlobalCoordinates, FString File);

	UFUNCTION(BlueprintCallable, Category = "LandscapeMesh")
	bool RegenerateMesh(double SplitNormalsAngle, EGridSplitDirection SplitDirection, double ApronWidth = 0.0, double ApronDepth = 0.0);

	static void RegisterAndCutLowerPriority(ALandscapeMesh* Mesh, double SplitNormalsAngle, EGridSplitDirection SplitDirection, double ApronWidth = 0.0, double ApronDepth = 0.0);

	static void Unregister(ALandscapeMesh* Mesh);

protected:
	virtual void Destroyed() override;
	virtual void BeginPlay() override;
	virtual void PostRegisterAllComponents() override;

	bool CookCollisionFromCurrentMesh();
	bool bHasCookedCollisionThisSession = false;

	FCriticalSection CookLock;
	FThreadSafeCounter64 MeshGenerationCounter;

	static bool IsPointCoveredByHigherOrEqualPriority(const FVector2D& Point, int Priority, ALandscapeMesh* Self);

	static FCriticalSection RegistryLock;
	static TArray<FRegisteredHeightmap> GRegisteredHeightmaps;
};
