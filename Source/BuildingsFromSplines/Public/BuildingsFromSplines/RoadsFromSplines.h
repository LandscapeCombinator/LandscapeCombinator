// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "BuildingsFromSplines/LogBuildingsFromSplines.h"
#include "BuildingsFromSplines/RoadGraph.h"
#include "LCCommon/LCGenerator.h"
#include "LCCommon/ActorSelection.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"

#include "RoadsFromSplines.generated.h"

struct FEdgeSplinePoints
{
	FVector StartLocal = FVector::ZeroVector;
	FVector StartTangentLocal = FVector::ZeroVector;
	FVector EndLocal = FVector::ZeroVector;
	FVector EndTangentLocal = FVector::ZeroVector;
	double TilingOffset = 0.0;
};

UENUM(BlueprintType)
enum class ERoadJunctionFillMode : uint8
{
	CurvyCorners   UMETA(DisplayName = "Curvy Corner Meshes"),
	StraightStubs  UMETA(DisplayName = "Straight Stub Meshes")
};

struct FRoadEdgeSplineData
{
	TWeakObjectPtr<USplineComponent> OriginalSplineComponent;
	TObjectPtr<USplineMeshComponent> MeshComponent;
	TOptional<FVector> BlendedStartTangent;
	TOptional<FVector> BlendedEndTangent;
	bool bMeshSpawnPending = false;
};

UCLASS(Blueprintable, PrioritizeCategories = "RoadsFromSplines")
class BUILDINGSFROMSPLINES_API ARoadsFromSplines : public AActor, public ILCGenerator
{
	GENERATED_BODY()

public:
	ARoadsFromSplines();

	virtual void Destroyed() override;

	/** Components */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Components",
		meta = (DisplayPriority = "0")
	)
	TObjectPtr<USceneComponent> EmptySceneComponent;


	/** Road & Intersection Meshes */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Meshes",
		meta = (DisplayPriority = "0")
	)
	TObjectPtr<UStaticMesh> RoadMesh;

	/* Mesh used to fill the curb-return arcs at junctions where more than 2 roads meet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Meshes",
		meta = (DisplayPriority = "1")
	)
	TObjectPtr<UStaticMesh> IntersectionMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Meshes",
		meta = (DisplayPriority = "2")
	)
	TEnumAsByte<ESplineMeshAxis::Type> RoadMeshAxis = ESplineMeshAxis::X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Meshes",
		meta = (DisplayPriority = "3")
	)
	double WidthScale = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Meshes",
		meta = (DisplayPriority = "4")
	)
	double HeightScale = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Meshes",
		meta = (DisplayPriority = "5")
	)
	double ZOffset = 0;

	/* Name of a scalar material parameter that multiplies UV tiling along the road,
		so dashes/markings stay evenly spaced even when a segment stretches or compresses.
		Leave "None" to disable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Meshes",
		meta = (DisplayPriority = "6")
	)
	FName UVTilingParameterName = "TilingY";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Meshes",
		meta = (DisplayPriority = "7")
	)
	FName TilingOffsetParameterName = "TilingOffset";

	/* Get the default tiling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Meshes",
		meta = (DisplayPriority = "8")
	)
	double DefaultRoadMeshLength = 800.0;

	/* Get the default tiling */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Meshes",
		meta = (DisplayPriority = "9")
	)
	double DefaultIntersectionMeshLength = 800.0;


	/** Landscape Adaptation */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Landscape",
		meta = (DisplayPriority = "0")
	)
	bool bAdaptSplineMeshRollToLandscape = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Landscape",
		meta = (EditCondition="bAdaptSplineMeshRollToLandscape", EditConditionHides, DisplayPriority = "1")
	)
	FActorSelection LandscapeSelection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Landscape",
		meta = (EditCondition="bAdaptSplineMeshRollToLandscape", EditConditionHides, DisplayPriority = "2")
	)
	double AdaptSplineMeshRollAlpha = 0.7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Landscape",
		meta = (EditCondition="bAdaptSplineMeshRollToLandscape", EditConditionHides, DisplayPriority = "3")
	)
	// in °
	double AdaptSplineMeshMaxAngle = 30;


	/** Spline Source & Graph Building */

	/* The tag of the actors containing spline components to search for. If None, all actors will be used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Graph",
		meta = (DisplayPriority = "0")
	)
	FName SplinesTag;

	/* The tag of the Spline Components to use to generate roads. If None, all spline components will be used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Graph",
		meta = (DisplayPriority = "1")
	)
	FName SplineComponentsTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Graph",
		meta = (DisplayPriority = "2")
	)
	bool bResampleSplines = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Graph",
		meta = (EditCondition = "bResampleSplines", EditConditionHides, DisplayPriority = "3")
	)
	double SplineMaxSpacing = 500.0;

	/* Points closer than this are considered the same graph node (and become a junction). */
	UPROPERTY(EditAnywhere, Category = "RoadsFromSplines|Graph",
		meta = (DisplayPriority = "4")
	)
	double JoinDistance = 200.0f;

	/* If true, every Generate call wipes everything and rebuilds from scratch.
	   If false (recommended), only new/removed splines are touched: fully incremental. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Graph",
		meta = (DisplayPriority = "5")
	)
	bool bDeleteOldRoadsWhenCreatingRoads = false;


	/** Junctions */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Junctions",
		meta = (DisplayPriority = "0")
	)
	ERoadJunctionFillMode JunctionFillMode = ERoadJunctionFillMode::CurvyCorners;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Junctions",
		meta = (DisplayPriority = "1")
	)
	double IntersectionSize = 500.0f;

	/* Corner fillets between two roads meeting at an angle sharper than this are skipped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Junctions",
		meta = (DisplayPriority = "2", ClampMin = "0", ClampMax = "180")
	)
	double MinCornerAngleDegrees = 30;

	/* Distance from the road centerline to the inner (junction-facing) edge of the
		IntersectionMesh, in cm. IntersectionMesh is usually just a curb/sidewalk strip
		near the edge of the road, not the full road width, so this is independent of
		how wide the road actually is -- set it to wherever the near edge of that mesh
		asset actually sits (e.g. for a 4m road with a 0.7m sidewalk strip starting at
		the road edge, that's 200 - 70 = 130).
		Used to align the junction interior fill boundary with the real edge of the
		spawned intersection meshes, and to find the curb-line point at corners sharp
		enough to skip a fillet mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Junctions",
		meta = (DisplayPriority = "3")
	)
	double JunctionCurbOffset = 130.0;

	/* How much the corner arcs bulge into the junction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Junctions",
		meta = (DisplayPriority = "4")
	)
	double CornerStraightness = 5;

	/* Z offset for corner meshes to prevent them from Z-fighting */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Junctions",
		meta = (DisplayPriority = "5")
	)
	double IntersectionStaggerZ = 1;

	/* If true, fill the empty interior of curvy junctions with a mesh that follows the
	   (possibly sloped) boundary formed by the corner fillet meshes. Ignored in straight-stub mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Junctions",
		meta = (DisplayPriority = "6")
	)
	bool bFillJunctions = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Junctions",
		meta = (DisplayPriority = "7", EditCondition = "bFillJunctions", EditConditionHides)
	)
	TObjectPtr<UMaterialInterface> JunctionFillMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Junctions",
		meta = (DisplayPriority = "8", EditCondition = "bFillJunctions", EditConditionHides)
	)
	double FillJunctionUVScale = 1.0f;

	/* Vertical thickness of the junction fill slab. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Junctions",
		meta = (DisplayPriority = "10", EditCondition = "bFillJunctions", EditConditionHides)
	)
	float JunctionFillHeight = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Junctions",
		meta = (DisplayPriority = "20", EditCondition = "bFillJunctions", EditConditionHides)
	)
	int InnerFillingNumSamples = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Junctions",
		meta = (DisplayPriority = "100", EditCondition = "bFillJunctions", EditConditionHides)
	)
	bool bShowDebugJunctionFill = false;


	/** Push Out Of Collision */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|PushOutOfCollision",
		meta = (DisplayPriority = "0")
	)
	bool bPushBuildingsOnSpawn = true;


	/** Debug */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RoadsFromSplines|Debug",
		meta = (DisplayPriority = "0")
	)
	bool bShowDebugGraph = false;


	/** Generation */

	UFUNCTION(BlueprintCallable, Category = "RoadsFromSplines",
		meta = (DisplayPriority = "0")
	)
	bool GenerateRoads(bool bIsUserInitiated);

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RoadsFromSplines",
		meta = (DisplayName = "Generate Roads", DisplayPriority = "1")
	)
	void GenerateRoadsEditor() { GenerateRoads(true); }

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RoadsFromSplines",
		meta = (DisplayPriority = "2", DisplayName = "Clear Roads")
	)
	void ClearRoads();

	virtual bool OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated) override;

	virtual TArray<UObject*> GetGeneratedObjects() const override;

	virtual bool Cleanup_Implementation(bool bSkipPrompt) override;

#if WITH_EDITOR
	virtual AActor* Duplicate(FName FromName, FName ToName) override;
#endif

	UFUNCTION(CallInEditor, BlueprintImplementableEvent, Category = "RoadsFromSplines",
		meta = (DisplayPriority = "3")
	)
	void OnSplineMeshCreated(USplineComponent *FromSplineComponent, USplineMeshComponent *SplineMeshCreated);

protected:

	/** Mesh Component Storage */

	UPROPERTY(DuplicateTransient)
	TArray<TObjectPtr<USplineMeshComponent>> SplineMeshComponents;

	UPROPERTY(DuplicateTransient)
	TArray<TObjectPtr<USplineMeshComponent>> CornerMeshComponents;

	UPROPERTY(DuplicateTransient)
	TMap<int32, TObjectPtr<UDynamicMeshComponent>> JunctionFillMeshComponents;

	TMap<int32, TArray<TObjectPtr<USplineMeshComponent>>> JunctionMeshComponents;


	/** Graph Data */

	FRoadGraph RoadGraph;

	TMap<TWeakObjectPtr<USplineComponent>, TArray<int32>> SplineToEdgeIndices;
	TMap<int32, FRoadEdgeSplineData> EdgeSplineData;

	FCollisionQueryParams LandscapeCollisionQueryParams;


	/** Graph & Edge Mesh Building */

	bool BuildEdgesForSpline(USplineComponent* OriginalSpline, const TArray<FVector>& Points, const TArray<FVector>& Tangents, bool bClosedLoop, TArray<int32>& OutNewEdgeIndices);
	bool BuildResampledPoints(USplineComponent* Source, float MaxSpacing, TArray<FVector>& OutPoints, TArray<FVector>& OutTangents) const;

	USplineMeshComponent* SpawnSplineMeshComponent(const FEdgeSplinePoints& Points, UStaticMesh* Mesh, const float DefaultMeshLength);
	USplineMeshComponent* DetachEdgeMesh(int32 EdgeIndex);
	void DestroyEdgeMesh(int32 EdgeIndex);
	bool CreateSplineMeshForEdge(int32 EdgeIndex, float ZStagger);

	void CreateMeshesForNewEdges(const TArray<int32>& EdgeIndices);
	bool EdgeTouchesJunction(int32 EdgeIndex) const;

	void UpdateBlendedTangentsAtNode(int32 NodeIndex);


	/** Junction Mesh Building */

	void CreateStraightJunctionMeshes(int32 JunctionNode);
	void CreateCurvyJunctionMeshes(int32 JunctionNode);
	void CreateMeshesForJunctions(const TArray<int32>& TouchedNodeIndices);

	void DestroyJunctionMeshes(int32 JunctionNode);
	void DestroyMeshesAtJunctionNodes(const TArray<int32>& TouchedNodeIndices);

	void SampleFilletInnerEdge(USplineMeshComponent* Mesh, const FVector& JunctionCenter, TArray<FVector>& OutPoints) const;
	void FillJunctionInterior(int32 JunctionNode, TArray<FVector> BoundaryWorld);


	/** Landscape Roll */

	bool ComputeLandscapeRoll(const FVector& LocationWorld, const FVector& TangentWorld, float& OutRoll) const;
	void ApplyLandscapeRoll(USplineMeshComponent* MeshComponent, const FEdgeSplinePoints& Points) const;


	/** Misc */

	void PushOverlappingBuildings(UPrimitiveComponent* MeshComponent) const;
	void DebugDrawGraph();
};
