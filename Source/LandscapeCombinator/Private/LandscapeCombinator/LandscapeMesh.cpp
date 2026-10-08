// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeMesh.h"
#include "LandscapeCombinator/LandscapeMeshSpawner.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "ConstrainedDelaunay2.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/World.h"
#include "Engine/CollisionProfile.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshUVFunctions.h"
#include "DynamicMesh/MeshNormals.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeLock.h"
#include "UObject/StrongObjectPtr.h"
#include "Stats/StatsMisc.h"
#include "HAL/Event.h"

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

FCriticalSection ALandscapeMesh::RegistryLock;
TArray<FRegisteredHeightmap> ALandscapeMesh::GRegisteredHeightmaps;

ALandscapeMesh::ALandscapeMesh()
{
	MeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetMobility(EComponentMobility::Static);
	MeshComponent->SetCollisionProfileName("BlockAll");
	MeshComponent->SetComplexAsSimpleCollisionEnabled(true);
	MeshComponent->bUseAsyncCooking = true;

	RootComponent = MeshComponent;
}

void ALandscapeMesh::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(MeshComponent)) MeshComponent->RecreatePhysicsState();
}

void ALandscapeMesh::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	// Register landscapes so that later landscapes crop them
	const UWorld* ThisWorld = GetWorld();
	if (IsValid(ThisWorld) && !ThisWorld->IsGameWorld())
	{
		const TArray<FRegisteredHeightmap> Entries = MakeRegistryEntries(this);
		if (!Entries.IsEmpty())
		{
			FScopeLock Lock(&RegistryLock);
			GRegisteredHeightmaps.RemoveAll([this](const FRegisteredHeightmap& Entry) {
				return !Entry.Mesh.IsValid() || Entry.Mesh.Get() == this;
			});
			GRegisteredHeightmaps.Append(Entries);
		}
	}

	if (bHasCookedCollisionThisSession) return;
	if (!IsValid(MeshComponent) || !IsValid(MeshComponent->GetDynamicMesh())) return;
	if (MeshComponent->GetDynamicMesh()->GetMeshRef().TriangleCount() == 0) return;

	bHasCookedCollisionThisSession = true;

	TWeakObjectPtr<ALandscapeMesh> WeakThis(this);
	Async(EAsyncExecution::TaskGraph, [WeakThis]()
	{
		if (ALandscapeMesh* StrongThis = WeakThis.Get())
		{
			StrongThis->CookCollisionFromCurrentMesh();
		}
	});
}

void ALandscapeMesh::Destroyed()
{
	MeshGenerationCounter.Increment();

	const UWorld* ThisWorld = GetWorld();
	const bool bTeardown = IsEngineExitRequested() || !IsValid(ThisWorld) || ThisWorld->bIsTearingDown;
	Unregister(this, !bTeardown);

	Super::Destroyed();
}

void ALandscapeMesh::Clear()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	Modify();

	{
		FScopeLock Lock(&HeightmapsLock);
		Heightmaps.Empty();
	}
	Unregister(this);

	if (IsValid(MeshComponent) && IsValid(MeshComponent->GetDynamicMesh()))
	{
		MeshComponent->GetDynamicMesh()->GetMeshRef().Clear();
		MeshComponent->MarkRenderStateDirty();
	}
}

bool ALandscapeMesh::AddHeightmap(int InPriority, FVector4d Coordinates, UGlobalCoordinates* GlobalCoordinates, FString File, FName OwnerName)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	TWeakObjectPtr<ALandscapeMesh> WeakThis(this);

	double LeftCoord = Coordinates[0];
	double RightCoord = Coordinates[1];
	double BottomCoord = Coordinates[2];
	double TopCoord = Coordinates[3];
	int InWidth = 0;
	int InHeight = 0;
	TArray<float> Data;

	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("ALandscapeMesh::AddHeightmap::ReadFile");
		if (!GDALInterface::ReadHeightmapFromFile(File, InWidth, InHeight, Data))
		{
			LCReporter::ShowError(LOCTEXT("HeightmapError", "Could not read heightmap from file: {0}"), FText::FromString(File));
			return false;
		}
	}

	TArray<FVector> NewPoints;
	FVector4d NewRect(DBL_MAX, -DBL_MAX, DBL_MAX, -DBL_MAX);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("ALandscapeMesh::AddHeightmap::BuildPoints");
		NewPoints.SetNum(InWidth * InHeight);
		for (int32 j = 0; j < InHeight; ++j)
		{
			for (int32 i = 0; i < InWidth; ++i)
			{
				double X = LeftCoord + (i + 0.5) * (RightCoord - LeftCoord) / InWidth;
				double Y = TopCoord - (j + 0.5) * (TopCoord - BottomCoord) / InHeight;
				FVector2D UnrealCoordinates;
				GlobalCoordinates->GetUnrealCoordinatesFromCRS(X, Y, UnrealCoordinates);
				float Z = Data[j * InWidth + i] * 100;
				FVector Point(UnrealCoordinates.X, UnrealCoordinates.Y, Z);
				NewPoints[i + j * InWidth] = Point;

				NewRect[0] = FMath::Min(NewRect[0], Point.X);
				NewRect[1] = FMath::Max(NewRect[1], Point.X);
				NewRect[2] = FMath::Min(NewRect[2], Point.Y);
				NewRect[3] = FMath::Max(NewRect[3], Point.Y);
			}
		}
	}

	if (!WeakThis.IsValid()) return false;

	FLandscapeMeshHeightmap NewHeightmap;
	NewHeightmap.Owner = OwnerName;
	NewHeightmap.SourceCoordinates = Coordinates;
	NewHeightmap.Priority = InPriority;
	NewHeightmap.Width = InWidth;
	NewHeightmap.Height = InHeight;
	NewHeightmap.Rect = NewRect;
	NewHeightmap.Points = MoveTemp(NewPoints);

	return Concurrency::RunOnGameThreadThrottledAndWait([WeakThis, OwnerName, NewHeightmap = MoveTemp(NewHeightmap)]() mutable {
		ALandscapeMesh* StrongThis = WeakThis.Get();
		if (!IsValid(StrongThis)) return false;

		FScopeLock Lock(&StrongThis->HeightmapsLock);

		if (OwnerName.IsNone())
		{
			StrongThis->Heightmaps.Reset();
		}

		StrongThis->Modify();
		StrongThis->Heightmaps.Add(MoveTemp(NewHeightmap));
		return true;
	});
}

bool ALandscapeMesh::HasHeightmap(FName OwnerName, const FVector4d& SourceCoordinates)
{
	FScopeLock Lock(&HeightmapsLock);
	return Heightmaps.ContainsByPredicate([&OwnerName, &SourceCoordinates](const FLandscapeMeshHeightmap& H) {
		return H.Owner == OwnerName && H.SourceCoordinates == SourceCoordinates;
	});
}

int32 ALandscapeMesh::RemoveHeightmaps(FName OwnerName)
{
	Modify();

	FScopeLock Lock(&HeightmapsLock);
	Heightmaps.RemoveAll([OwnerName](const FLandscapeMeshHeightmap& H) { return H.Owner == OwnerName; });
	return Heightmaps.Num();
}

bool ALandscapeMesh::CookCollisionFromCurrentMesh()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	FScopeLock Lock(&CookLock);

	TWeakObjectPtr<ALandscapeMesh> WeakThis(this);
	const int64 ThisGeneration = MeshGenerationCounter.GetValue();

	// wait for the async collision cook without blocking the game thread
	TSharedPtr<FEvent, ESPMode::ThreadSafe> CookDoneEvent = MakeShareable(
		FPlatformProcess::GetSynchEventFromPool(false),
		[](FEvent* Ev) { FPlatformProcess::ReturnSynchEventToPool(Ev); }
	);
	TSharedPtr<bool, ESPMode::ThreadSafe> bCookSuccess = MakeShared<bool, ESPMode::ThreadSafe>(false);

	bool bCommitted = Concurrency::RunOnGameThreadThrottledAndWait([WeakThis, ThisGeneration, CookDoneEvent, bCookSuccess]()
	{
		ALandscapeMesh* StrongThis = WeakThis.Get();
		if (!IsValid(StrongThis) || !IsValid(StrongThis->MeshComponent)) return false;
		if (StrongThis->MeshGenerationCounter.GetValue() != ThisGeneration) return false;

		UBodySetup* BodySetup = StrongThis->MeshComponent->GetBodySetup();
		if (!IsValid(BodySetup)) return false;

		BodySetup->BodySetupGuid = FGuid::NewGuid();
		BodySetup->CollisionTraceFlag = StrongThis->MeshComponent->CollisionType;
		BodySetup->bHasCookedCollisionData = true;
		BodySetup->InvalidatePhysicsData();

		BodySetup->CreatePhysicsMeshesAsync(
			FOnAsyncPhysicsCookFinished::CreateLambda([CookDoneEvent, bCookSuccess](bool bSuccess)
			{
				*bCookSuccess = bSuccess;
				CookDoneEvent->Trigger();
			})
		);

		return true;
	});

	if (!bCommitted) return false;
	CookDoneEvent->Wait();

	return Concurrency::RunOnGameThreadThrottledAndWait([WeakThis, ThisGeneration, bCookSuccess]()
	{
		ALandscapeMesh* StrongThis = WeakThis.Get();
		if (!IsValid(StrongThis) || !IsValid(StrongThis->MeshComponent)) return false;
		if (StrongThis->MeshGenerationCounter.GetValue() != ThisGeneration) return false;

		StrongThis->MeshComponent->RecreatePhysicsState();

		return true;
	});
}

bool ALandscapeMesh::IsPointCoveredByHigherOrEqualPriority(const FVector2D& Point, int Priority, ALandscapeMesh* Self)
{
	FScopeLock Lock(&RegistryLock);

	const FName SelfName = IsValid(Self) ? Self->GetFName() : NAME_None;

	for (const FRegisteredHeightmap& Entry : GRegisteredHeightmaps)
	{
		ALandscapeMesh* EntryMesh = Entry.Mesh.Get();
		if (!IsValid(EntryMesh) || EntryMesh == Self) continue;
		if (Entry.Priority < Priority) continue;
		if (Entry.Priority == Priority && EntryMesh->GetFName().Compare(SelfName) < 0) continue;

		if (Point.X >= Entry.Rect[0] && Point.X <= Entry.Rect[1] &&
			Point.Y >= Entry.Rect[2] && Point.Y <= Entry.Rect[3])
		{
			return true;
		}
	}

	return false;
}

TArray<FRegisteredHeightmap> ALandscapeMesh::MakeRegistryEntries(ALandscapeMesh* Mesh)
{
	TArray<FRegisteredHeightmap> Entries;

	FScopeLock Lock(&Mesh->HeightmapsLock);
	for (const FLandscapeMeshHeightmap& Heightmap : Mesh->Heightmaps)
	{
		FRegisteredHeightmap Entry;
		Entry.Priority = Heightmap.Priority;
		Entry.Rect = Heightmap.Rect;
		Entry.Mesh = Mesh;
		Entries.Add(Entry);
	}
	return Entries;
}

void ALandscapeMesh::CollectMeshesCroppedBy(const TArray<FRegisteredHeightmap>& Entries, ALandscapeMesh* Skip, TArray<TWeakObjectPtr<ALandscapeMesh>>& OutMeshes)
{
	for (const FRegisteredHeightmap& Entry : GRegisteredHeightmaps)
	{
		if (!Entry.Mesh.IsValid() || Entry.Mesh.Get() == Skip) continue;

		for (const FRegisteredHeightmap& Other : Entries)
		{
			if (Entry.Priority > Other.Priority) continue;

			const bool bOverlaps =
			Entry.Rect[0] <= Other.Rect[1] && Entry.Rect[1] >= Other.Rect[0] &&
			Entry.Rect[2] <= Other.Rect[3] && Entry.Rect[3] >= Other.Rect[2];

			if (bOverlaps)
			{
				OutMeshes.AddUnique(Entry.Mesh);
				break;
			}
		}
	}
}

void ALandscapeMesh::RegenerateWithOwnSettings(const TArray<TWeakObjectPtr<ALandscapeMesh>>& Meshes, const FLastMeshSettings& Fallback, bool bAsync)
{
	auto Work = [Meshes, Fallback]()
	{
		for (const TWeakObjectPtr<ALandscapeMesh>& WeakMesh : Meshes)
		{
			ALandscapeMesh* Mesh = WeakMesh.Get();
			if (!IsValid(Mesh)) continue;

			FLastMeshSettings Settings = Mesh->GetLastSettings();
			if (!Settings.bValid) Settings = Fallback;
			if (!Settings.bValid)
			{
				UE_LOG(LogLandscapeCombinator, Warning, TEXT("Cannot rebuild %s: its generation settings are unknown. Please regenerate it manually."), *Mesh->GetActorNameOrLabel());
				continue;
			}

			Mesh->RegenerateMesh(Settings.SplitNormalsAngle, Settings.SplitDirection, Settings.ApronWidth, Settings.ApronDepth);
		}
	};

	if (bAsync) Concurrency::RunAsync(Work);
	else Work();
}

void ALandscapeMesh::RegisterAndCutLowerPriority(ALandscapeMesh* Mesh, double SplitNormalsAngle, EGridSplitDirection SplitDirection, double ApronWidth, double ApronDepth)
{
	if (!IsValid(Mesh)) return;

	const TArray<FRegisteredHeightmap> NewEntries = MakeRegistryEntries(Mesh);

	TArray<FRegisteredHeightmap> OldEntries;
	TArray<TWeakObjectPtr<ALandscapeMesh>> MeshesToRebuild;

	{
		FScopeLock Lock(&RegistryLock);

		for (const FRegisteredHeightmap& Entry : GRegisteredHeightmaps)
		{
			if (Entry.Mesh.Get() == Mesh) OldEntries.Add(Entry);
		}

		GRegisteredHeightmaps.RemoveAll([Mesh](const FRegisteredHeightmap& Entry) {
			return !Entry.Mesh.IsValid() || Entry.Mesh.Get() == Mesh;
		});

		GRegisteredHeightmaps.Append(NewEntries);
		CollectMeshesCroppedBy(NewEntries, Mesh, MeshesToRebuild);
		CollectMeshesCroppedBy(OldEntries, Mesh, MeshesToRebuild);
	}

	FLastMeshSettings Fallback;
	Fallback.bValid = true;
	Fallback.SplitNormalsAngle = SplitNormalsAngle;
	Fallback.SplitDirection = SplitDirection;
	Fallback.ApronWidth = ApronWidth;
	Fallback.ApronDepth = ApronDepth;

	RegenerateWithOwnSettings(MeshesToRebuild, Fallback, false);
}

FLastMeshSettings ALandscapeMesh::GetLastSettings()
{
	FScopeLock Lock(&HeightmapsLock);
	return LastSettings;
}

struct FLandscapeMeshGrid
{
	const FLandscapeMeshHeightmap& Heightmap;
	TArray<bool> bCovered;
	TArray<bool> QuadSkipped;
	TArray<int32> VertexIds;

	explicit FLandscapeMeshGrid(const FLandscapeMeshHeightmap& InHeightmap) : Heightmap(InHeightmap)
	{
		bCovered.Init(false, Heightmap.Points.Num());
		QuadSkipped.Init(false, FMath::Max(0, (Heightmap.Width - 1) * (Heightmap.Height - 1)));
		VertexIds.Init(-1, Heightmap.Points.Num());
	}

	int32 GetOrAddVertex(FDynamicMesh3& Mesh, const FTransform& WorldToMesh, int32 GridIndex)
	{
		if (VertexIds[GridIndex] == -1)
		{
			VertexIds[GridIndex] = Mesh.AppendVertex(WorldToMesh.TransformPosition(Heightmap.Points[GridIndex]));
		}
		return VertexIds[GridIndex];
	}
};

bool ALandscapeMesh::IsCoveredBySiblingHeightmap(const TArray<FLandscapeMeshHeightmap>& Siblings, int32 SelfIndex, const FVector2D& Point)
{
	const int32 SelfPriority = Siblings[SelfIndex].Priority;
	for (int32 OtherIndex = 0; OtherIndex < Siblings.Num(); ++OtherIndex)
	{
		if (OtherIndex == SelfIndex) continue;
		const FLandscapeMeshHeightmap& Other = Siblings[OtherIndex];
		if (Other.Priority < SelfPriority) continue;
		if (Other.Priority == SelfPriority && OtherIndex < SelfIndex) continue;

		if (Point.X >= Other.Rect[0] && Point.X <= Other.Rect[1] &&
			Point.Y >= Other.Rect[2] && Point.Y <= Other.Rect[3])
		{
			return true;
		}
	}
	return false;
}

void ALandscapeMesh::ComputeCoverage(FLandscapeMeshGrid& Grid, int32 HeightmapIndex, const TArray<FLandscapeMeshHeightmap>& Siblings)
{
	const TArray<FVector>& Points = Grid.Heightmap.Points;
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		const FVector2D Point2D(Points[Index].X, Points[Index].Y);
		Grid.bCovered[Index] =
			IsPointCoveredByHigherOrEqualPriority(Point2D, Grid.Heightmap.Priority, this) ||
			IsCoveredBySiblingHeightmap(Siblings, HeightmapIndex, Point2D);
	}
}

void ALandscapeMesh::BuildGridForHeightmap(FDynamicMesh3& Mesh, FLandscapeMeshGrid& Grid, const FTransform& WorldToMesh, EGridSplitDirection SplitDirection)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ALandscapeMesh::RegenerateMesh::BuildGridMesh");

	const int32 Width = Grid.Heightmap.Width;
	const int32 Height = Grid.Heightmap.Height;

	for (int32 j = 0; j < Height - 1; ++j)
	{
		for (int32 i = 0; i < Width - 1; ++i)
		{
			int32 TopLeft = i + j * Width;
			int32 TopRight = (i + 1) + j * Width;
			int32 BottomLeft = i + (j + 1) * Width;
			int32 BottomRight = (i + 1) + (j + 1) * Width;

			bool bSkipQuad = Grid.bCovered[TopLeft] || Grid.bCovered[TopRight] || Grid.bCovered[BottomLeft] || Grid.bCovered[BottomRight];
			Grid.QuadSkipped[i + j * (Width - 1)] = bSkipQuad;

			if (bSkipQuad) continue;

			int32 A = Grid.GetOrAddVertex(Mesh, WorldToMesh, TopLeft);
			int32 B = Grid.GetOrAddVertex(Mesh, WorldToMesh, TopRight);
			int32 C = Grid.GetOrAddVertex(Mesh, WorldToMesh, BottomLeft);
			int32 D = Grid.GetOrAddVertex(Mesh, WorldToMesh, BottomRight);

			bool bSplitForward = (SplitDirection == EGridSplitDirection::Forward) ||
				(SplitDirection == EGridSplitDirection::Checkerboard && (i + j) % 2 == 0);

			if (bSplitForward)
			{
				Mesh.AppendTriangle(A, D, B);
				Mesh.AppendTriangle(A, C, D);
			}
			else
			{
				Mesh.AppendTriangle(A, C, B);
				Mesh.AppendTriangle(C, D, B);
			}
		}
	}
}

void ALandscapeMesh::BuildApronForHeightmap(FDynamicMesh3& Mesh, FLandscapeMeshGrid& Grid, const FTransform& WorldToMesh, double ApronWidth, double ApronDepth)
{
	const TArray<FVector>& Points = Grid.Heightmap.Points;
	const int32 Width = Grid.Heightmap.Width;
	const int32 Height = Grid.Heightmap.Height;

	if (ApronWidth <= 0 || Width <= 1 || Height <= 1) return;

	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ALandscapeMesh::RegenerateMesh::BuildApron");

	auto IsQuadSkipped = [&](int32 qi, int32 qj) -> bool
	{
		if (qi < 0 || qi >= Width - 1 || qj < 0 || qj >= Height - 1) return true;
		return Grid.QuadSkipped[qi + qj * (Width - 1)];
	};

	struct FBoundaryEdge { int32 VaGrid; int32 VbGrid; };
	TArray<FBoundaryEdge> BoundaryEdges;

	TArray<int8> OutwardXSign, OutwardYSign;
	OutwardXSign.Init(0, Points.Num());
	OutwardYSign.Init(0, Points.Num());

	auto AddBoundaryEdge = [&](int32 qi, int32 qj, int32 VaGrid, int32 VbGrid)
	{
		const FVector& Va = Points[VaGrid];
		const FVector& Vb = Points[VbGrid];

		FVector2D EdgeDir(Vb.X - Va.X, Vb.Y - Va.Y);
		if (!EdgeDir.Normalize()) return;

		FVector2D Perp(-EdgeDir.Y, EdgeDir.X);

		FVector QuadCenter = 0.25 * (
			Points[qi + qj * Width] + Points[(qi + 1) + qj * Width] +
			Points[qi + (qj + 1) * Width] + Points[(qi + 1) + (qj + 1) * Width]);
		FVector2D Mid = 0.5 * (FVector2D(Va.X, Va.Y) + FVector2D(Vb.X, Vb.Y));
		FVector2D ToCenter = FVector2D(QuadCenter.X, QuadCenter.Y) - Mid;

		FVector2D Outward = (FVector2D::DotProduct(Perp, ToCenter) > 0.0) ? -Perp : Perp;

		int8 SignX = (Outward.X > 0.5) ? 1 : (Outward.X < -0.5) ? -1 : 0;
		int8 SignY = (Outward.Y > 0.5) ? 1 : (Outward.Y < -0.5) ? -1 : 0;

		double Cross2D = EdgeDir.X * Outward.Y - EdgeDir.Y * Outward.X;
		int32 FinalVaGrid = VaGrid;
		int32 FinalVbGrid = VbGrid;
		if (Cross2D <= 0.0)
		{
			FinalVaGrid = VbGrid;
			FinalVbGrid = VaGrid;
		}

		BoundaryEdges.Add({ FinalVaGrid, FinalVbGrid });
		if (SignX != 0) { OutwardXSign[VaGrid] = SignX; OutwardXSign[VbGrid] = SignX; }
		if (SignY != 0) { OutwardYSign[VaGrid] = SignY; OutwardYSign[VbGrid] = SignY; }
	};

	for (int32 j = 0; j < Height - 1; ++j)
	{
		for (int32 i = 0; i < Width - 1; ++i)
		{
			if (IsQuadSkipped(i, j)) continue;

			int32 TopLeft = i + j * Width;
			int32 TopRight = (i + 1) + j * Width;
			int32 BottomLeft = i + (j + 1) * Width;
			int32 BottomRight = (i + 1) + (j + 1) * Width;

			if (IsQuadSkipped(i - 1, j)) AddBoundaryEdge(i, j, TopLeft, BottomLeft);
			if (IsQuadSkipped(i + 1, j)) AddBoundaryEdge(i, j, TopRight, BottomRight);
			if (IsQuadSkipped(i, j - 1)) AddBoundaryEdge(i, j, TopLeft, TopRight);
			if (IsQuadSkipped(i, j + 1)) AddBoundaryEdge(i, j, BottomLeft, BottomRight);
		}
	}

	TArray<int32> ApronVertexIds;
	ApronVertexIds.Init(-1, Points.Num());

	auto GetOrAddApronVertex = [&](int32 GridIndex) -> int32
	{
		if (ApronVertexIds[GridIndex] == -1)
		{
			FVector2D Dir(OutwardXSign[GridIndex], OutwardYSign[GridIndex]);
			if (Dir.IsNearlyZero()) Dir = FVector2D(1, 0);
			FVector ApronPos = Points[GridIndex] + FVector(Dir.X, Dir.Y, 0.0) * ApronWidth - FVector(0, 0, ApronDepth);
			ApronVertexIds[GridIndex] = Mesh.AppendVertex(WorldToMesh.TransformPosition(ApronPos));
		}
		return ApronVertexIds[GridIndex];
	};

	for (const FBoundaryEdge& Edge : BoundaryEdges)
	{
		int32 Va = Grid.GetOrAddVertex(Mesh, WorldToMesh, Edge.VaGrid);
		int32 Vb = Grid.GetOrAddVertex(Mesh, WorldToMesh, Edge.VbGrid);
		int32 ApronA = GetOrAddApronVertex(Edge.VaGrid);
		int32 ApronB = GetOrAddApronVertex(Edge.VbGrid);

		Mesh.AppendTriangle(Va, ApronA, ApronB);
		Mesh.AppendTriangle(Va, ApronB, Vb);
	}
}

void ALandscapeMesh::FillGapsWithCDT(FDynamicMesh3& Mesh, const FTransform& WorldToMesh, const TArray<FLandscapeMeshHeightmap>& AllHeightmaps)
{
	if (Mesh.TriangleCount() == 0) return;

	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ALandscapeMesh::RegenerateMesh::ConstrainedDelaunay");

	auto Area2 = [&Mesh](int32 A, int32 B, int32 C) -> double
	{
		const FVector3d PA = Mesh.GetVertex(A);
		const FVector3d PB = Mesh.GetVertex(B);
		const FVector3d PC = Mesh.GetVertex(C);
		return (PB.X - PA.X) * (PC.Y - PA.Y) - (PB.Y - PA.Y) * (PC.X - PA.X);
	};

	auto SortedKey = [](int32 A, int32 B, int32 C)
	{
		if (A > B) Swap(A, B);
		if (B > C) Swap(B, C);
		if (A > B) Swap(A, B);
		return FIntVector(A, B, C);
	};

	FConstrainedDelaunay2d CDT;
	CDT.bOrientedEdges = false;

	CDT.Vertices.SetNum(Mesh.MaxVertexID());
	for (int32 VertexId : Mesh.VertexIndicesItr())
	{
		const FVector3d P = Mesh.GetVertex(VertexId);
		CDT.Vertices[VertexId] = FVector2d(P.X, P.Y);
	}

	for (int32 EdgeId : Mesh.EdgeIndicesItr())
		CDT.Edges.Add(Mesh.GetEdgeV(EdgeId));

	TSet<FIntVector> ExistingTriangles;
	double ExpectedSign = 0.0;
	for (int32 TriangleId : Mesh.TriangleIndicesItr())
	{
		const FIndex3i T = Mesh.GetTriangle(TriangleId);
		ExistingTriangles.Add(SortedKey(T.A, T.B, T.C));
		if (ExpectedSign == 0.0) ExpectedSign = Area2(T.A, T.B, T.C);
	}

	int32 MinPriority = MAX_int32;
	for (const FLandscapeMeshHeightmap& H : AllHeightmaps) MinPriority = FMath::Min(MinPriority, H.Priority);

	const bool bCDTSuccess = CDT.Triangulate([&](const TArray<FVector2d>&, const FIndex3i& T)
	{
		const FVector3d Centroid = (Mesh.GetVertex(T.A) + Mesh.GetVertex(T.B) + Mesh.GetVertex(T.C)) / 3.0;
		const FVector World = WorldToMesh.InverseTransformPosition(Centroid);
		return !IsPointCoveredByHigherOrEqualPriority(FVector2D(World.X, World.Y), MinPriority, this);
	});

	if (!bCDTSuccess)
	{
		UE_LOG(LogLandscapeCombinator, Warning, TEXT("Constrained Delaunay triangulation failed in %s, the gaps between heightmaps are not filled."), *GetActorNameOrLabel());
		return;
	}

	for (const FIndex3i& T : CDT.Triangles)
	{
		if (ExistingTriangles.Contains(SortedKey(T.A, T.B, T.C))) continue;

		const double Area = Area2(T.A, T.B, T.C);
		if (Area == 0.0) continue;

		if ((Area > 0.0) == (ExpectedSign > 0.0)) Mesh.AppendTriangle(T.A, T.B, T.C);
		else Mesh.AppendTriangle(T.A, T.C, T.B);
	}
}

bool ALandscapeMesh::RegenerateMesh(double SplitNormalsAngle, EGridSplitDirection SplitDirection, double ApronWidth, double ApronDepth)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	TWeakObjectPtr<ALandscapeMesh> WeakThis(this);
	TArray<FLandscapeMeshHeightmap> LocalHeightmaps;
	int64 ThisGeneration = 0;
	{
		FScopeLock Lock(&HeightmapsLock);
		LocalHeightmaps = Heightmaps;
		ThisGeneration = MeshGenerationCounter.Increment();
	}

	if (LocalHeightmaps.IsEmpty() ||
		LocalHeightmaps.ContainsByPredicate([](const FLandscapeMeshHeightmap& H) { return H.Points.IsEmpty() || H.Width <= 0 || H.Height <= 0; }))
	{
		LCReporter::ShowError(LOCTEXT("NoHeightmap", "There is no heightmap in the Landscape Mesh, cannot generate"));
		return false;
	}

	{
		FScopeLock Lock(&HeightmapsLock);
		LastSettings.bValid = true;
		LastSettings.SplitNormalsAngle = SplitNormalsAngle;
		LastSettings.SplitDirection = SplitDirection;
		LastSettings.ApronWidth = ApronWidth;
		LastSettings.ApronDepth = ApronDepth;
	}

	const bool bMultiHeightmap = LocalHeightmaps.Num() > 1;
	if (bMultiHeightmap) ApronWidth = 0;

	FTransform WorldToMesh;
	bool bGotTransform = Concurrency::RunOnGameThreadAndWait([WeakThis, &WorldToMesh]() {
		if (!WeakThis.IsValid() || !IsValid(WeakThis->MeshComponent)) return false;
		WorldToMesh = WeakThis->MeshComponent->GetComponentTransform().Inverse();
		return true;
	});
	if (!bGotTransform) return false;

	FDynamicMesh3 NewMesh;

	for (int32 HeightmapIndex = 0; HeightmapIndex < LocalHeightmaps.Num(); ++HeightmapIndex)
	{
		FLandscapeMeshGrid Grid(LocalHeightmaps[HeightmapIndex]);
		ComputeCoverage(Grid, HeightmapIndex, LocalHeightmaps);
		BuildGridForHeightmap(NewMesh, Grid, WorldToMesh, SplitDirection);
		BuildApronForHeightmap(NewMesh, Grid, WorldToMesh, ApronWidth, ApronDepth);
	}

	if (bMultiHeightmap) FillGapsWithCDT(NewMesh, WorldToMesh, LocalHeightmaps);

	ALandscapeMesh* StrongThis = WeakThis.Get();
	if (!IsValid(StrongThis) || !IsValid(StrongThis->MeshComponent) || !IsValid(StrongThis->MeshComponent->GetDynamicMesh()))
	{
		LCReporter::ShowError(LOCTEXT("MeshComponentError", "Dynamic mesh is not valid"));
		return false;
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("ALandscapeMesh::RegenerateMesh::ComputeSplitNormals");

		UDynamicMesh* ScratchMesh = nullptr;
		bool bAllocated = Concurrency::RunOnGameThreadAndWait([&ScratchMesh]() {
			ScratchMesh = NewObject<UDynamicMesh>();
			if (IsValid(ScratchMesh))
			{
				ScratchMesh->AddToRoot(); // no GC
				return true;
			}
			return false;
		});
		if (!bAllocated) return false;
		if (!IsValid(ScratchMesh)) return false;

		ScratchMesh->GetMeshRef() = MoveTemp(NewMesh);
		UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(
			ScratchMesh, FGeometryScriptSplitNormalsOptions(), FGeometryScriptCalculateNormalsOptions()
		);
		NewMesh = MoveTemp(ScratchMesh->GetMeshRef());
		Concurrency::RunOnGameThread([ScratchMesh]() {
			if (IsValid(ScratchMesh))
			{
				ScratchMesh->RemoveFromRoot();
			}
		});
	}

	TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe> NewMeshPtr =
		MakeShared<FDynamicMesh3, ESPMode::ThreadSafe>(MoveTemp(NewMesh));

	bool bCommitted = Concurrency::RunOnGameThreadThrottledAndWait([WeakThis, NewMeshPtr, ThisGeneration]()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("ALandscapeMesh::RegenerateMesh::CommitMeshOnGameThread");

		ALandscapeMesh* StrongThis = WeakThis.Get();
		if (!IsValid(StrongThis) || !IsValid(StrongThis->MeshComponent) || !IsValid(StrongThis->MeshComponent->GetDynamicMesh()))
			return false;

		if (StrongThis->MeshGenerationCounter.GetValue() != ThisGeneration)
			return false;

		StrongThis->Modify();
		StrongThis->MeshComponent->Modify();
		StrongThis->MeshComponent->GetDynamicMesh()->GetMeshRef() = MoveTemp(*NewMeshPtr);
		StrongThis->MeshComponent->NotifyMeshUpdated();
		StrongThis->MeshComponent->MarkPackageDirty();

		return true;
	});

	if (!bCommitted) return MeshGenerationCounter.GetValue() != ThisGeneration;

	bHasCookedCollisionThisSession = true;
	if (!CookCollisionFromCurrentMesh()) return MeshGenerationCounter.GetValue() != ThisGeneration;
	return true;
}

void ALandscapeMesh::Unregister(ALandscapeMesh* Mesh, bool bRestoreCropped)
{
	TArray<FRegisteredHeightmap> Removed;
	TArray<TWeakObjectPtr<ALandscapeMesh>> ToRestore;

	{
		FScopeLock Lock(&RegistryLock);

		if (IsValid(Mesh))
		{
			for (const FRegisteredHeightmap& Entry : GRegisteredHeightmaps)
			{
				if (Entry.Mesh.Get() == Mesh) Removed.Add(Entry);
			}
		}

		GRegisteredHeightmaps.RemoveAll([Mesh](const FRegisteredHeightmap& Entry) {
			return !Entry.Mesh.IsValid() || Entry.Mesh.Get() == Mesh;
		});

		if (bRestoreCropped) CollectMeshesCroppedBy(Removed, Mesh, ToRestore);
	}

	if (!ToRestore.IsEmpty()) RegenerateWithOwnSettings(ToRestore, FLastMeshSettings(), true);
}

#undef LOCTEXT_NAMESPACE
