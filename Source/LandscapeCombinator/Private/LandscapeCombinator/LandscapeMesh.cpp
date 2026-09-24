// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeMesh.h"
#include "LandscapeCombinator/LandscapeMeshSpawner.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "GDALInterface/GDALInterface.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"

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
	Unregister(this);
	Super::Destroyed();
}

void ALandscapeMesh::Clear()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	Modify();

	Points.Empty();
	Width = 0;
	Height = 0;
	Unregister(this);

	if (IsValid(MeshComponent) && IsValid(MeshComponent->GetDynamicMesh()))
	{
		MeshComponent->GetDynamicMesh()->GetMeshRef().Clear();
		MeshComponent->MarkRenderStateDirty();
	}
}

bool ALandscapeMesh::AddHeightmap(int InPriority, FVector4d Coordinates, UGlobalCoordinates* GlobalCoordinates, FString File)
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

	return Concurrency::RunOnGameThreadThrottledAndWait([WeakThis, InPriority, InWidth, InHeight, NewPoints, NewRect]() mutable {
		ALandscapeMesh* StrongThis = WeakThis.Get();
		if (!IsValid(StrongThis)) return false;

		StrongThis->Modify();
		StrongThis->Points = MoveTemp(NewPoints);
		StrongThis->Width = InWidth;
		StrongThis->Height = InHeight;
		StrongThis->Priority = InPriority;
		StrongThis->Rect = NewRect;
		return true;
	});
}

bool ALandscapeMesh::CookCollisionFromCurrentMesh()
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	FScopeLock Lock(&CookLock);

	TWeakObjectPtr<ALandscapeMesh> WeakThis(this);
	const int64 ThisGeneration = MeshGenerationCounter.Increment();

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

	const uint32 SelfId = IsValid(Self) ? Self->GetUniqueID() : 0;

	for (const FRegisteredHeightmap& Entry : GRegisteredHeightmaps)
	{
		ALandscapeMesh* EntryMesh = Entry.Mesh.Get();
		if (!IsValid(EntryMesh) || EntryMesh == Self) continue;
		if (Entry.Priority < Priority) continue;
		if (Entry.Priority == Priority && EntryMesh->GetUniqueID() < SelfId) continue;

		if (Point.X >= Entry.Rect[0] && Point.X <= Entry.Rect[1] &&
			Point.Y >= Entry.Rect[2] && Point.Y <= Entry.Rect[3])
		{
			return true;
		}
	}

	return false;
}

void ALandscapeMesh::RegisterAndCutLowerPriority(ALandscapeMesh* Mesh, double SplitNormalsAngle, EGridSplitDirection SplitDirection, double ApronWidth, double ApronDepth)
{
	if (!IsValid(Mesh)) return;

	TArray<TWeakObjectPtr<ALandscapeMesh>> MeshesToCut;

	{
		FScopeLock Lock(&RegistryLock);

		GRegisteredHeightmaps.RemoveAll([Mesh](const FRegisteredHeightmap& Entry) {
			return !Entry.Mesh.IsValid() || Entry.Mesh.Get() == Mesh;
		});

		FRegisteredHeightmap NewEntry;
		NewEntry.Priority = Mesh->Priority;
		NewEntry.Rect = Mesh->Rect;
		NewEntry.Mesh = Mesh;
		GRegisteredHeightmaps.Add(NewEntry);

		for (const FRegisteredHeightmap& Entry : GRegisteredHeightmaps)
		{
			if (Entry.Mesh.Get() == Mesh) continue;
			if (Entry.Priority > Mesh->Priority) continue;

			bool bOverlaps =
				Entry.Rect[0] <= Mesh->Rect[1] && Entry.Rect[1] >= Mesh->Rect[0] &&
				Entry.Rect[2] <= Mesh->Rect[3] && Entry.Rect[3] >= Mesh->Rect[2];

			if (bOverlaps)
			{
				ALandscapeMesh* EntryMesh = Entry.Mesh.Get();
				MeshesToCut.Add(Entry.Mesh);
			}
		}
	}

	for (TWeakObjectPtr<ALandscapeMesh>& WeakMesh : MeshesToCut)
	{
		if (ALandscapeMesh* StrongMesh = WeakMesh.Get())
		{
			StrongMesh->RegenerateMesh(SplitNormalsAngle, SplitDirection, ApronWidth, ApronDepth);
		}
	}
}

bool ALandscapeMesh::RegenerateMesh(double SplitNormalsAngle, EGridSplitDirection SplitDirection, double ApronWidth, double ApronDepth)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__);

	TWeakObjectPtr<ALandscapeMesh> WeakThis(this);

	if (Points.IsEmpty() || Width <= 0 || Height <= 0)
	{
		LCReporter::ShowError(LOCTEXT("NoHeightmap", "There is no heightmap in the Landscape Mesh, cannot generate"));
		return false;
	}

	TArray<FVector> LocalPoints = Points;
	int LocalWidth = Width;
	int LocalHeight = Height;
	int LocalPriority = Priority;

	TArray<bool> bCovered;
	bCovered.SetNum(LocalPoints.Num());
	for (int32 Index = 0; Index < LocalPoints.Num(); ++Index)
	{
		FVector2D Point2D(LocalPoints[Index].X, LocalPoints[Index].Y);
		bCovered[Index] = IsPointCoveredByHigherOrEqualPriority(Point2D, LocalPriority, this);
	}

	FTransform WorldToMesh;
	bool bGotTransform = Concurrency::RunOnGameThreadAndWait([WeakThis, &WorldToMesh]() {
		if (!WeakThis.IsValid() || !IsValid(WeakThis->MeshComponent)) return false;
		WorldToMesh = WeakThis->MeshComponent->GetComponentTransform().Inverse();
		return true;
	});
	if (!bGotTransform) return false;

	FDynamicMesh3 NewMesh;
	int32 TotalQuads = 0;
	int32 SkippedQuads = 0;

	TArray<int32> VertexIds;
	VertexIds.Init(-1, LocalPoints.Num());

	auto GetOrAddVertex = [&](int32 GridIndex) -> int32
	{
		if (VertexIds[GridIndex] == -1)
		{
			VertexIds[GridIndex] = NewMesh.AppendVertex(WorldToMesh.TransformPosition(LocalPoints[GridIndex]));
		}
		return VertexIds[GridIndex];
	};

	TArray<bool> QuadSkipped;
	QuadSkipped.Init(false, FMath::Max(0, (LocalWidth - 1) * (LocalHeight - 1)));

	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("ALandscapeMesh::RegenerateMesh::BuildGridMesh");

		for (int32 j = 0; j < LocalHeight - 1; ++j)
		{
			for (int32 i = 0; i < LocalWidth - 1; ++i)
			{
				int32 TopLeft = i + j * LocalWidth;
				int32 TopRight = (i + 1) + j * LocalWidth;
				int32 BottomLeft = i + (j + 1) * LocalWidth;
				int32 BottomRight = (i + 1) + (j + 1) * LocalWidth;

				bool bSkipQuad = bCovered[TopLeft] || bCovered[TopRight] || bCovered[BottomLeft] || bCovered[BottomRight];
				QuadSkipped[i + j * (LocalWidth - 1)] = bSkipQuad;

				TotalQuads++;
				if (bSkipQuad)
				{
					SkippedQuads++;
					continue;
				}

				int32 A = GetOrAddVertex(TopLeft);
				int32 B = GetOrAddVertex(TopRight);
				int32 C = GetOrAddVertex(BottomLeft);
				int32 D = GetOrAddVertex(BottomRight);

				bool bSplitForward = (SplitDirection == EGridSplitDirection::Forward) ||
					(SplitDirection == EGridSplitDirection::Checkerboard && (i + j) % 2 == 0);

				if (bSplitForward)
				{
					NewMesh.AppendTriangle(A, D, B);
					NewMesh.AppendTriangle(A, C, D);
				}
				else
				{
					NewMesh.AppendTriangle(A, C, B);
					NewMesh.AppendTriangle(C, D, B);
				}
			}
		}
	}

	if (ApronWidth > 0 && LocalWidth > 1 && LocalHeight > 1)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("ALandscapeMesh::RegenerateMesh::BuildApron");

		auto IsQuadSkipped = [&](int32 qi, int32 qj) -> bool
		{
			if (qi < 0 || qi >= LocalWidth - 1 || qj < 0 || qj >= LocalHeight - 1) return true;
			return QuadSkipped[qi + qj * (LocalWidth - 1)];
		};

		struct FBoundaryEdge { int32 VaGrid; int32 VbGrid; };
		TArray<FBoundaryEdge> BoundaryEdges;

		TArray<int8> OutwardXSign, OutwardYSign;
		OutwardXSign.Init(0, LocalPoints.Num());
		OutwardYSign.Init(0, LocalPoints.Num());

		auto AddBoundaryEdge = [&](int32 qi, int32 qj, int32 VaGrid, int32 VbGrid)
		{
			const FVector& Va = LocalPoints[VaGrid];
			const FVector& Vb = LocalPoints[VbGrid];

			FVector2D EdgeDir(Vb.X - Va.X, Vb.Y - Va.Y);
			if (!EdgeDir.Normalize()) return;

			FVector2D Perp(-EdgeDir.Y, EdgeDir.X);

			FVector QuadCenter = 0.25 * (
				LocalPoints[qi + qj * LocalWidth] + LocalPoints[(qi + 1) + qj * LocalWidth] +
				LocalPoints[qi + (qj + 1) * LocalWidth] + LocalPoints[(qi + 1) + (qj + 1) * LocalWidth]);
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

		for (int32 j = 0; j < LocalHeight - 1; ++j)
		{
			for (int32 i = 0; i < LocalWidth - 1; ++i)
			{
				if (IsQuadSkipped(i, j)) continue;

				int32 TopLeft = i + j * LocalWidth;
				int32 TopRight = (i + 1) + j * LocalWidth;
				int32 BottomLeft = i + (j + 1) * LocalWidth;
				int32 BottomRight = (i + 1) + (j + 1) * LocalWidth;

				if (IsQuadSkipped(i - 1, j)) AddBoundaryEdge(i, j, TopLeft, BottomLeft);
				if (IsQuadSkipped(i + 1, j)) AddBoundaryEdge(i, j, TopRight, BottomRight);
				if (IsQuadSkipped(i, j - 1)) AddBoundaryEdge(i, j, TopLeft, TopRight);
				if (IsQuadSkipped(i, j + 1)) AddBoundaryEdge(i, j, BottomLeft, BottomRight);
			}
		}

		TArray<int32> ApronVertexIds;
		ApronVertexIds.Init(-1, LocalPoints.Num());

		auto GetOrAddApronVertex = [&](int32 GridIndex) -> int32
		{
			if (ApronVertexIds[GridIndex] == -1)
			{
				FVector2D Dir(OutwardXSign[GridIndex], OutwardYSign[GridIndex]);
				if (Dir.IsNearlyZero()) Dir = FVector2D(1, 0);
				FVector ApronPos = LocalPoints[GridIndex] + FVector(Dir.X, Dir.Y, 0.0) * ApronWidth - FVector(0, 0, ApronDepth);
				ApronVertexIds[GridIndex] = NewMesh.AppendVertex(WorldToMesh.TransformPosition(ApronPos));
			}
			return ApronVertexIds[GridIndex];
		};

		for (const FBoundaryEdge& Edge : BoundaryEdges)
		{
			int32 Va = GetOrAddVertex(Edge.VaGrid);
			int32 Vb = GetOrAddVertex(Edge.VbGrid);
			int32 ApronA = GetOrAddApronVertex(Edge.VaGrid);
			int32 ApronB = GetOrAddApronVertex(Edge.VbGrid);

			NewMesh.AppendTriangle(Va, ApronA, ApronB);
			NewMesh.AppendTriangle(Va, ApronB, Vb);
		}
	}

	{
		int32 CoveredCount = 0;
		for (bool b : bCovered) {if (b) CoveredCount++; }
	}

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

	const int64 ThisGeneration = MeshGenerationCounter.Increment();

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

	if (!bCommitted) return false;

	bHasCookedCollisionThisSession = true;
	return CookCollisionFromCurrentMesh();
}

void ALandscapeMesh::Unregister(ALandscapeMesh* Mesh)
{
	FScopeLock Lock(&RegistryLock);
	GRegisteredHeightmaps.RemoveAll([Mesh](const FRegisteredHeightmap& Entry) {
		return !Entry.Mesh.IsValid() || Entry.Mesh.Get() == Mesh;
	});
}

#undef LOCTEXT_NAMESPACE
