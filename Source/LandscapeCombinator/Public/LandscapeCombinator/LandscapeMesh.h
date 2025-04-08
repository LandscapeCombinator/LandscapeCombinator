// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Coordinates/GlobalCoordinates.h"
#include "Components/DynamicMeshComponent.h"
#include "Polygon2.h"
#include "LandscapeMesh.generated.h"

using namespace UE::Geometry;

USTRUCT(BlueprintType)
struct FHeightmap
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FHeightmap")
	TSet<FVector> Points;

	TPolygon2<double> Boundary;

	void UpdateBoundary();
};

USTRUCT(BlueprintType)
struct FHeightmaps
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FHeightmaps")
	TArray<FHeightmap> Heightmaps;
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
	UDynamicMeshComponent* MeshComponent;

	UPROPERTY(BlueprintReadWrite, Transient, Category = "LandscapeMesh")
	TMap<int, FHeightmaps> PriorityToHeightmaps;

	bool AddHeightmap(int Priority, FVector4d Coordinates, UGlobalCoordinates* GlobalCoordinates, FString File);

	UFUNCTION(BlueprintCallable, Category = "LandscapeMesh")
	bool RegenerateMesh(double SplitNormalsAngle);

};
