// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeMesh.h"
#include "LandscapeCombinator/LandscapeMeshSpawner.h"
#include "LCReporter/LCReporter.h"
#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/Concurrency.h"

#include "CompGeom/Delaunay2.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshUVFunctions.h"
#include "DynamicMesh/MeshNormals.h"
#include "Math/ConvexHull2d.h"
#include "Kismet/GameplayStatics.h"

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void FHeightmap::UpdateBoundary()
{
	TArray<FVector2D> Points2D;
	TArray<int32> OutIndices;

	for (const FVector& Point : Points) Points2D.Add(FVector2D(Point.X, Point.Y));

	ConvexHull2D::ComputeConvexHull(Points2D, OutIndices);

	Boundary.SetVertices(TArray<FVector2D>());
	for (int32 Index : OutIndices) Boundary.AppendVertex(Points2D[Index]);
}

ALandscapeMesh::ALandscapeMesh()
{
	MeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetMobility(EComponentMobility::Static);
	MeshComponent->SetCollisionProfileName("BlockAll");
	MeshComponent->SetComplexAsSimpleCollisionEnabled(true);

	RootComponent = MeshComponent;
}

void ALandscapeMesh::Clear()
{
	PriorityToHeightmaps.Empty();
	if (IsValid(MeshComponent) && IsValid(MeshComponent->GetDynamicMesh()))
	{
		MeshComponent->GetDynamicMesh()->GetMeshRef().Clear();
		MeshComponent->MarkRenderStateDirty();
	}

	TArray<AActor*> LandscapeMeshSpawners;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ALandscapeMeshSpawner::StaticClass(), LandscapeMeshSpawners);

	for (AActor* LandscapeMeshSpawner0 : LandscapeMeshSpawners)
	{
		ALandscapeMeshSpawner* LandscapeMeshSpawner = Cast<ALandscapeMeshSpawner>(LandscapeMeshSpawner0);
		if (
			IsValid(LandscapeMeshSpawner) &&
			LandscapeMeshSpawner->bReuseExistingMesh &&
			LandscapeMeshSpawner->ExistingLandscapeMesh.GetActor(GetWorld()) == this &&
			LandscapeMeshSpawner->PositionBasedGeneration
		)
		{
			LandscapeMeshSpawner->PositionBasedGeneration->ClearGeneratedTilesCache();
		}
	}
}

bool ALandscapeMesh::AddHeightmap(int Priority, FVector4d Coordinates, UGlobalCoordinates* GlobalCoordinates, FString File)
{
	double LeftCoord = Coordinates[0];
	double RightCoord = Coordinates[1];
	double BottomCoord = Coordinates[2];
	double TopCoord = Coordinates[3];
	FHeightmap Heightmap;
	int Width, Height;
	TArray<float> Data;
	if (!GDALInterface::ReadHeightmapFromFile(File, Width, Height, Data))
	{
		ULCReporter::ShowError(LOCTEXT("HeightmapError", "Could not read heightmap from file: {0}"), FText::FromString(File));
		return false;
	}
	for (int32 j = 0; j < Height; ++j)
	{
		for (int32 i = 0; i < Width; ++i)
		{
			double X = LeftCoord + (i + 0.5) * (RightCoord - LeftCoord) / Width;
			double Y = TopCoord - (j + 0.5) * (TopCoord - BottomCoord) / Height;
			FVector2D UnrealCoordinates;
			GlobalCoordinates->GetUnrealCoordinatesFromCRS(X, Y, UnrealCoordinates);
			float Z = Data[j * Width + i] * 100;
			FVector Point(UnrealCoordinates.X, UnrealCoordinates.Y, Z);
			Heightmap.Points.Add(Point);
		}
	}
	Heightmap.UpdateBoundary();
	FHeightmaps& Heightmaps = PriorityToHeightmaps.FindOrAdd(Priority);
	Heightmaps.Heightmaps.Add(Heightmap);
	return true;
}

bool ALandscapeMesh::RegenerateMesh(double SplitNormalsAngle)
{
	TSet<FVector> Points;
	TArray<int32> Priorities;
	PriorityToHeightmaps.GetKeys(Priorities);
	if (PriorityToHeightmaps.IsEmpty())
	{
		ULCReporter::ShowError(LOCTEXT("NoHeightmaps", "There are no heightmaps in the Landscape Mesh, cannot generate"));
		return false;
	}

	Priorities.Sort();
	for (int32 Priority : Priorities)
	{
		FHeightmaps& Heightmaps = PriorityToHeightmaps[Priority];
		for (FHeightmap& Heightmap : Heightmaps.Heightmaps)
		{
			// Make a copy to avoid modifying Points while iterating on it
			TSet<FVector> InitialPoints(Points);
			for (FVector Point : InitialPoints)
				if (Heightmap.Boundary.Contains({ Point.X, Point.Y })) Points.Remove(Point);
		}

		for (FHeightmap& Heightmap : Heightmaps.Heightmaps)
			for (FVector Point : Heightmap.Points)
				Points.Add(Point);
	}

	if (Points.IsEmpty())
	{
		ULCReporter::ShowError(LOCTEXT("NoPoint", "Trying to generate empty mesh, something went wrong in the Landscape Mesh Spawner"));
		return false;
	}
	
	FDelaunay2 Delaunay;
	Delaunay.bAutomaticallyFixEdgesToDuplicateVertices = true;
	
	TArray<FVector> PointsArray = Points.Array();
	TArray<TVector2<double>> Points2D;
	TArray<FIndex2i> Edges;

	for (const FVector& Point : PointsArray)
	{
		Points2D.Add({ Point[0], Point[1]});
	}

	if (!Delaunay.Triangulate(Points2D, Edges))
	{
		ULCReporter::ShowError(LOCTEXT("DelaunayError", "Delaunay triangulation failed"));
		return false;
	}

	if (!IsValid(MeshComponent) || !IsValid(MeshComponent->GetDynamicMesh()))
	{
		ULCReporter::ShowError(LOCTEXT("MeshComponentError", "Dynamic mesh is not valid"));
		return false;
	}

	// Validate the triangles
	for (auto &Triangle : Delaunay.GetTriangles())
	{
		if (
			Triangle.A < 0 || Triangle.B < 0 || Triangle.C < 0 ||
			Triangle.A >= PointsArray.Num() || Triangle.B >= PointsArray.Num() || Triangle.C >= PointsArray.Num())
		{
			ULCReporter::ShowError(
				FText::Format(
					LOCTEXT("InvalidTriangleError", "Delaunay returned an invalid triangle {0} {1} {2} ({3} vertices)"),
					FText::AsNumber(Triangle.A),
					FText::AsNumber(Triangle.B),
					FText::AsNumber(Triangle.C),
					FText::AsNumber(PointsArray.Num())
				)
			);
			return false;
		}
	}

	FDynamicMesh3 NewMesh;
	FTransform WorldToMesh = MeshComponent->GetComponentTransform().Inverse();
	for (auto &Point : Points) NewMesh.AppendVertex(WorldToMesh.TransformPosition(Point));
	for (auto &Triangle : Delaunay.GetTriangles()) NewMesh.AppendTriangle(Triangle.A, Triangle.C, Triangle.B);
	NewMesh.EnableAttributes();
	FMeshNormals::InitializeOverlayToPerVertexNormals(NewMesh.Attributes()->PrimaryNormals(), false);

	Concurrency::RunOnGameThreadAndWait([this, &NewMesh]() {;
		FDynamicMesh3 &TargetMesh = MeshComponent->GetDynamicMesh()->GetMeshRef();
		TargetMesh = MoveTemp(NewMesh);
		UGeometryScriptLibrary_MeshNormalsFunctions::SetPerVertexNormals(MeshComponent->GetDynamicMesh());
	});

	return true;
}

#undef LOCTEXT_NAMESPACE
