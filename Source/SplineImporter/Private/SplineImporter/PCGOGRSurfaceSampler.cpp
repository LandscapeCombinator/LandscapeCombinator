// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "SplineImporter/PCGOGRSurfaceSampler.h"
#include "SplineImporter/LogSplineImporter.h"
#include "SplineImporter/OGRArealIndex.h"

#include "Coordinates/LevelCoordinates.h"
#include "LandscapeUtils/LandscapeUtils.h"

#include "PCGComponent.h"
#if !UE_VERSION_OLDER_THAN(5, 7, 0)
#include "PCGGraphExecutionStateInterface.h"
#endif
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/PCGActorSelector.h"

#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGOGRSurfaceSampler)

#define LOCTEXT_NAMESPACE "FSplineImporterModule"

FPCGElementPtr UPCGOGRSurfaceSamplerSettings::CreateElement() const
{
	return MakeShared<FPCGOGRSurfaceSamplerElement>();
}

#if !UE_VERSION_OLDER_THAN(5, 7, 0)

#if WITH_EDITOR
void UPCGOGRSurfaceSamplerSettings::GetStaticTrackedKeys(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const
{
	if (ExclusionRadiusCm <= 0.0 || SplinesTags.IsEmpty()) return;
	for (const FName& Tag : SplinesTags) OutKeysToSettings.FindOrAdd(FPCGSelectionKey(Tag)).Emplace(this, /*bCulling=*/false);
}
#endif

namespace LCOGRSampler
{
	constexpr double Pi = 3.14159265358979323846;

	FORCEINLINE uint64 Mix64(uint64 X)
	{
		X += 0x9E3779B97F4A7C15ull;
		X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
		X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
		return X ^ (X >> 31);
	}

	FORCEINLINE double Unit(uint64 X) { return double(X >> 11) * (1.0 / 9007199254740992.0); }

	FORCEINLINE uint64 HashCell(uint64 Salt, int64 Row, int64 Col) { return Mix64(Mix64(Salt ^ uint64(Row)) + uint64(Col)); }

	struct FSphereLattice
	{
		double RadiusCm = 637100000.0;
		double SpacingCm = 1000.0;
		double DLatRad = 0.0;
		double DLatDeg = 0.0;
		int64 MaxRow = 0;

		void Init(double InRadiusCm, double InSpacingCm)
		{
			RadiusCm = FMath::Max(InRadiusCm, 1.0);
			SpacingCm = FMath::Max(InSpacingCm, 1.0);
			DLatRad = SpacingCm / RadiusCm;
			DLatDeg = FMath::RadiansToDegrees(DLatRad);
			MaxRow = FMath::FloorToInt64(0.5 * Pi / DLatRad + 1e-9);
		}

		int64 NumInRow(int64 Row) const
		{
			const double Circumference = 2.0 * Pi * RadiusCm * FMath::Cos(double(Row) * DLatRad);
			return FMath::Max<int64>(1, FMath::RoundToInt64(Circumference / SpacingCm));
		}
	};

	static bool GetCap(const FVector& PlanetCenter, const FVector& Center, double Radius, FVector& OutDirection, double& OutAngle)
	{
		const FVector V = Center - PlanetCenter;
		const double D = V.Size();
		if (D <= Radius || D < 1e-6) return false;
		OutDirection = V / D;
		OutAngle = FMath::Asin(Radius / D);
		return true;
	}

	class FSplineExclusionIndex
	{
	public:
		void Init(double InRadiusCm, double InCellSizeCm)
		{
			Radius = InRadiusCm;
			RadiusSq = InRadiusCm * InRadiusCm;
			InvCell = 1.0 / FMath::Max(InCellSizeCm, 1.0);
		}

		void AddSegment(const FVector& A, const FVector& B)
		{
			const int32 Id = Segments.Add({ A, B });
			const FIntVector Lo = ToCell(A.ComponentMin(B) - FVector(Radius));
			const FIntVector Hi = ToCell(A.ComponentMax(B) + FVector(Radius));
			for (int32 Z = Lo.Z; Z <= Hi.Z; ++Z)
				for (int32 Y = Lo.Y; Y <= Hi.Y; ++Y)
					for (int32 X = Lo.X; X <= Hi.X; ++X)
						Grid.FindOrAdd(FIntVector(X, Y, Z)).Add(Id);
		}

		bool IsNear(const FVector& P) const
		{
			const TArray<int32>* Ids = Grid.Find(ToCell(P));
			if (!Ids) return false;
			for (const int32 Id : *Ids)
			{
				if (FMath::PointDistToSegmentSquared(P, Segments[Id].A, Segments[Id].B) <= RadiusSq) return true;
			}
			return false;
		}

	private:
		struct FSegment { FVector A; FVector B; };

		FIntVector ToCell(const FVector& P) const
		{
			return FIntVector(FMath::FloorToInt(P.X * InvCell), FMath::FloorToInt(P.Y * InvCell), FMath::FloorToInt(P.Z * InvCell));
		}

		double Radius = 0.0;
		double RadiusSq = 0.0;
		double InvCell = 1.0;
		TArray<FSegment> Segments;
		TMap<FIntVector, TArray<int32>> Grid;
	};
}

struct FOGRSurfaceSamplerState
{
	enum class EStage : uint8 { Enumerate, Trace, Done };

	struct FCandidate
	{
		double LonDeg;
		double LatDeg;
		int32 Seed;
	};

	EStage Stage = EStage::Enumerate;

	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UGlobalCoordinates> Coordinates;
	UPCGPointData* OutData = nullptr;

	FBox CellBox = FBox(ForceInit);
	double MinAltitudeCm = 0.0;
	double MaxAltitudeCm = 0.0;
	double Jitter = 0.0;
	uint64 SeedSalt = 0;
	double TimeSliceSeconds = 0.004;
	LCOGRSampler::FSphereLattice Lattice;

	double LatMin = 0.0, LatMax = 0.0, LonMin = 0.0, LonMax = 0.0;
	bool bAllLongitudes = false;
	int64 Row = 0;
	int64 RowHi = -1;
	bool bRowReady = false;
	int64 RowCount = 1;
	double RowDLonDeg = 360.0;
	int64 Col = 0;
	int64 ColHi = -1;

	TSharedPtr<const FOGRArealIndex> AreaIndex;
	LCOGRSampler::FSplineExclusionIndex Exclusion;
	bool bHasExclusion = false;

	FCollisionQueryParams QueryParams;
	TArray<FCandidate> Candidates;
	int32 NextCandidate = 0;
};

namespace
{
	using EStage = FOGRSurfaceSamplerState::EStage;

	bool IsInsideHalfOpen(const FBox& Box, const FVector& P)
	{
		return P.X >= Box.Min.X && P.X < Box.Max.X
			&& P.Y >= Box.Min.Y && P.Y < Box.Max.Y
			&& P.Z >= Box.Min.Z && P.Z < Box.Max.Z;
	}

	void BuildSplineIndex(UWorld* World, const UGlobalCoordinates* G, const UPCGOGRSurfaceSamplerSettings* Settings, FOGRSurfaceSamplerState& S)
	{
		using namespace LCOGRSampler;

		const double R = G->BodyRadiusCm;
		const FVector PlanetCenter = G->SphereCenter();
		const double Radius = Settings->ExclusionRadiusCm;
		const double Step = FMath::Clamp(Radius * 0.5, 50.0, 500.0);

		S.Exclusion.Init(Radius, FMath::Max(2.0 * Radius, 100.0));
		S.bHasExclusion = true;

		FVector CellDirection;
		double CellAngle;
		if (!GetCap(PlanetCenter, S.CellBox.GetCenter(), S.CellBox.GetExtent().Size(), CellDirection, CellAngle)) return;

		const double MarginAngle = (Radius + Step) / R;
		const double NearCos = FMath::Cos(FMath::Min(CellAngle + MarginAngle, Pi));

		for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
		{
			AActor* Actor = *ActorIt;
			if (!IsValid(Actor)) continue;
			if (!Settings->SplinesTags.IsEmpty() && !Settings->SplinesTags.ContainsByPredicate([Actor](const FName& Tag) { return Actor->ActorHasTag(Tag); })) continue;

			TArray<USplineComponent*> Splines;
			Actor->GetComponents<USplineComponent>(Splines);
			for (USplineComponent* Spline : Splines)
			{
				if (!IsValid(Spline) || Spline->GetNumberOfSplinePoints() < 2) continue;
				if (!Settings->SplineComponentsTag.IsNone() && !Spline->ComponentHasTag(Settings->SplineComponentsTag)) continue;

				FVector BoundsDirection;
				double BoundsAngle;
				if (GetCap(PlanetCenter, Spline->Bounds.Origin, Spline->Bounds.SphereRadius, BoundsDirection, BoundsAngle))
				{
					const double Between = FMath::Acos(FMath::Clamp(FVector::DotProduct(CellDirection, BoundsDirection), -1.0, 1.0));
					if (Between > CellAngle + BoundsAngle + MarginAngle) continue;
				}

				const double Length = Spline->GetSplineLength();
				const int32 NumSteps = FMath::Max(1, FMath::CeilToInt(Length / Step));

				FVector Previous = FVector::ZeroVector;
				bool bPreviousNear = false;
				for (int32 i = 0; i <= NumSteps; ++i)
				{
					const double Distance = FMath::Min(double(i) * Step, Length);
					const FVector Location = Spline->GetLocationAtDistanceAlongSpline(static_cast<float>(Distance), ESplineCoordinateSpace::World);

					const FVector Radial = Location - PlanetCenter;
					const double RadialLength = Radial.Size();
					if (RadialLength < 1.0) continue;

					const FVector Direction = Radial / RadialLength;
					const FVector Projected = PlanetCenter + Direction * R;
					const bool bNear = FVector::DotProduct(Direction, CellDirection) >= NearCos;

					if (i > 0 && (bNear || bPreviousNear)) S.Exclusion.AddSegment(Previous, Projected);
					Previous = Projected;
					bPreviousNear = bNear;
				}
			}
		}
	}

	bool InitializeState(FPCGOGRSurfaceSamplerContext* Context, const UPCGOGRSurfaceSamplerSettings* Settings, FOGRSurfaceSamplerState& S)
	{
		using namespace LCOGRSampler;

		if (!Context->ExecutionSource.IsValid())
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NoExecutionSource", "Unable to get the PCG execution source."));
			return false;
		}
		const IPCGGraphExecutionState& ExecState = Context->ExecutionSource->GetExecutionState();

		UWorld* World = ExecState.GetWorld();
		if (!IsValid(World))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NoWorld", "Unable to get the world."));
			return false;
		}

		UGlobalCoordinates* G = IsValid(Settings->GeometryActor)
			? ALevelCoordinates::GetGlobalCoordinatesFor(Settings->GeometryActor, false)
			: ALevelCoordinates::GetGlobalCoordinates(World, false);
		if (!IsValid(G))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NoGlobalCoordinates", "Unable to find the LevelCoordinates of this level."));
			return false;
		}
		if (!G->IsSphere())
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NotSphere", "OGR Surface Sampler requires the Sphere body model in LevelCoordinates."));
			return false;
		}

		if (ExecState.IsLocalSource() && ExecState.Use2DGrid())
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("Grid2D", "OGR Surface Sampler needs 3D cells: disable '2D Grid' in the graph settings."));
			return false;
		}

		const UPCGSpatialData* SelfData = Cast<UPCGSpatialData>(ExecState.GetSelfData());
		if (!SelfData)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NoSelfData", "Unable to get the bounds of the generated cell."));
			return false;
		}
		S.CellBox = SelfData->GetBounds();
		if (!S.CellBox.IsValid)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("InvalidBounds", "The generated cell has invalid bounds."));
			return false;
		}

		if (!(Settings->SpacingCm >= 1.0))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("BadSpacing", "SpacingCm must be at least 1."));
			return false;
		}

		if (IsValid(Settings->GeometryActor))
		{
			S.AreaIndex = Settings->GeometryActor->GetArealIndex();
			if (!S.AreaIndex.IsValid())
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context,
					FText::Format(
						LOCTEXT("NoGeometryData", "Unable to get OGR Geometry. Please make sure the OGRGeometry actor {0} is initialized"),
						FText::FromString(Settings->GeometryActor->GetActorNameOrLabel())
					)
				);
				return false;
			}
		}

		if (!LandscapeUtils::CustomCollisionQueryParams(World, Settings->SurfaceSelection, S.QueryParams))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NoSurface", "Unable to get the surface actors. Please check the Surface Selection."));
			return false;
		}

		S.World = World;
		S.Coordinates = G;
		S.Jitter = FMath::Clamp(Settings->Jitter, 0.0, 1.0);
		S.SeedSalt = Mix64(uint64(uint32(Settings->GlobalSeed)) + 0x5851F42D4C957F2Dull);
		S.TimeSliceSeconds = FMath::Max(Settings->TimeSliceMs, 0.5) * 0.001;
		S.MinAltitudeCm = Settings->MinAltitudeCm;
		S.MaxAltitudeCm = FMath::Max(Settings->MaxAltitudeCm, S.MinAltitudeCm + 1.0);

		UPCGPointData* OutData = NewObject<UPCGPointData>();
		S.OutData = OutData;
		FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
		Output.Data = OutData;

		const double R = G->BodyRadiusCm;
		const FVector PlanetCenter = G->SphereCenter();
		const FVector& Min = S.CellBox.Min;
		const FVector& Max = S.CellBox.Max;
		const double MinDistance = FMath::Sqrt(S.CellBox.ComputeSquaredDistanceToPoint(PlanetCenter));
		const FVector FarCorner(
			FMath::Abs(PlanetCenter.X - Min.X) > FMath::Abs(PlanetCenter.X - Max.X) ? Min.X : Max.X,
			FMath::Abs(PlanetCenter.Y - Min.Y) > FMath::Abs(PlanetCenter.Y - Max.Y) ? Min.Y : Max.Y,
			FMath::Abs(PlanetCenter.Z - Min.Z) > FMath::Abs(PlanetCenter.Z - Max.Z) ? Min.Z : Max.Z
		);
		const double MaxDistance = FVector::Dist(PlanetCenter, FarCorner);
		if (R + S.MaxAltitudeCm < MinDistance || R + S.MinAltitudeCm > MaxDistance)
		{
			S.Stage = EStage::Done;
			return true;
		}

		if (!G->GetSphereFootprint(S.CellBox, S.LatMin, S.LatMax, S.LonMin, S.LonMax))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("CellTooBig", "The generated cell contains the planet center. Use a partitioned graph with '2D Grid' disabled and a smaller grid size."));
			return false;
		}

		S.Lattice.Init(R, Settings->SpacingCm);
		S.bAllLongitudes = (S.LonMax - S.LonMin) >= 360.0 - 1e-9;
		S.Row = FMath::Max(-S.Lattice.MaxRow, FMath::FloorToInt64(S.LatMin / S.Lattice.DLatDeg) - 1);
		S.RowHi = FMath::Min(S.Lattice.MaxRow, FMath::CeilToInt64(S.LatMax / S.Lattice.DLatDeg) + 1);

		const double LatExtentRad = FMath::DegreesToRadians(S.LatMax - S.LatMin);
		const double LonExtentRad = FMath::DegreesToRadians(FMath::Min(S.LonMax - S.LonMin, 360.0));
		const double ClosestToEquator = (S.LatMin <= 0.0 && S.LatMax >= 0.0) ? 0.0 : FMath::Min(FMath::Abs(S.LatMin), FMath::Abs(S.LatMax));
		const double Estimate =
			(LatExtentRad * R / Settings->SpacingCm) *
			(LonExtentRad * FMath::Cos(FMath::DegreesToRadians(ClosestToEquator)) * R / Settings->SpacingCm);
		if (Estimate > double(Settings->MaxCandidatesPerCell))
		{
			PCGE_LOG_C(Error, GraphAndLog, Context,
				FText::Format(
					LOCTEXT("TooManyCandidates", "About {0} lattice points for one cell. Increase SpacingCm or use smaller cells (limit: MaxCandidatesPerCell)."),
					FText::AsNumber(int64(Estimate))
				)
			);
			return false;
		}

		if (Settings->ExclusionRadiusCm > 0.0) BuildSplineIndex(World, G, Settings, S);

		return true;
	}

	void ProcessCandidate(FOGRSurfaceSamplerState& S, const UGlobalCoordinates& G, int64 Row, int64 UnwrappedCol)
	{
		using namespace LCOGRSampler;

		const int64 N = S.RowCount;
		const int64 Col = ((UnwrappedCol % N) + N) % N;

		const uint64 H = HashCell(S.SeedSalt, Row, Col);
		const double JLat = Unit(H) - 0.5;
		const double JLon = Unit(Mix64(H)) - 0.5;

		const double LatDeg = FMath::Clamp((double(Row) + JLat * S.Jitter) * S.Lattice.DLatDeg, -90.0, 90.0);
		double LonDeg = -180.0 + (double(Col) + 0.5 + JLon * S.Jitter) * S.RowDLonDeg;
		if (LonDeg >= 180.0) LonDeg -= 360.0;

		const FVector A = G.ToWorld(LonDeg, LatDeg, S.MinAltitudeCm);
		const FVector B = G.ToWorld(LonDeg, LatDeg, S.MaxAltitudeCm);
		if (!FMath::LineBoxIntersection(S.CellBox, A, B, B - A)) return;

		if (S.AreaIndex.IsValid() && !S.AreaIndex->ContainsLonLat(LonDeg, LatDeg)) return;

		if (S.bHasExclusion && S.Exclusion.IsNear(G.ToWorld(LonDeg, LatDeg, 0.0))) return;

		S.Candidates.Add({ LonDeg, LatDeg, int32(H & 0x7FFFFFFFull) });
	}

	bool EnumerateCandidates(FOGRSurfaceSamplerState& S, double Deadline)
	{
		const UGlobalCoordinates* G = S.Coordinates.Get();
		if (!G)
		{
			S.Candidates.Reset();
			S.Stage = EStage::Done;
			return true;
		}

		int32 Counter = 0;
		while (S.Row <= S.RowHi)
		{
			if (!S.bRowReady)
			{
				S.RowCount = S.Lattice.NumInRow(S.Row);
				S.RowDLonDeg = 360.0 / double(S.RowCount);
				if (S.bAllLongitudes)
				{
					S.Col = 0;
					S.ColHi = S.RowCount - 1;
				}
				else
				{
					S.Col = FMath::FloorToInt64((S.LonMin + 180.0) / S.RowDLonDeg) - 1;
					S.ColHi = FMath::CeilToInt64((S.LonMax + 180.0) / S.RowDLonDeg) + 1;
					if (S.ColHi - S.Col + 1 >= S.RowCount)
					{
						S.Col = 0;
						S.ColHi = S.RowCount - 1;
					}
				}
				S.bRowReady = true;
			}

			while (S.Col <= S.ColHi)
			{
				ProcessCandidate(S, *G, S.Row, S.Col++);
				if ((++Counter & 127) == 0 && FPlatformTime::Seconds() > Deadline) return false;
			}

			++S.Row;
			S.bRowReady = false;
		}
		return true;
	}

	bool TraceCandidates(FOGRSurfaceSamplerState& S, double Deadline)
	{
		UWorld* World = S.World.Get();
		const UGlobalCoordinates* G = S.Coordinates.Get();
		if (!World || !G || !S.OutData)
		{
			S.NextCandidate = S.Candidates.Num();
			return true;
		}

		TArray<FPCGPoint>& Points = S.OutData->GetMutablePoints();

		while (S.NextCandidate < S.Candidates.Num())
		{
			const FOGRSurfaceSamplerState::FCandidate& Candidate = S.Candidates[S.NextCandidate++];

			const FVector Start = G->ToWorld(Candidate.LonDeg, Candidate.LatDeg, 0.0);
			FVector Hit;
			if (LandscapeUtils::GetSurfaceLocation(World, S.QueryParams, G, Start, Hit) && IsInsideHalfOpen(S.CellBox, Hit))
			{
				FPCGPoint& Point = Points.Emplace_GetRef(FTransform(G->Alignment(Hit), Hit), 1.0f, Candidate.Seed);
				Point.BoundsMin = FVector(-50.0);
				Point.BoundsMax = FVector(50.0);
				Point.Steepness = 1.0f;
			}

			if (FPlatformTime::Seconds() > Deadline) return S.NextCandidate >= S.Candidates.Num();
		}
		return true;
	}
}

#endif // !UE_VERSION_OLDER_THAN(5, 7, 0)

bool FPCGOGRSurfaceSamplerElement::ExecuteInternal(FPCGContext* InContext) const
{
#if UE_VERSION_OLDER_THAN(5, 7, 0)
	PCGE_LOG_C(Error, GraphAndLog, InContext, LOCTEXT("UnsupportedVersion", "The OGR Surface Sampler node requires Unreal Engine 5.7 or newer."));
	return true;
#else
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGOGRSurfaceSamplerElement::ExecuteInternal);

	FPCGOGRSurfaceSamplerContext* Context = static_cast<FPCGOGRSurfaceSamplerContext*>(InContext);
	const UPCGOGRSurfaceSamplerSettings* Settings = Context->GetInputSettings<UPCGOGRSurfaceSamplerSettings>();
	check(Settings);

	if (!Context->State.IsValid())
	{
		Context->State = MakeShared<FOGRSurfaceSamplerState>();
		if (!InitializeState(Context, Settings, *Context->State)) return true;
	}

	FOGRSurfaceSamplerState& S = *Context->State;
	const double Deadline = FPlatformTime::Seconds() + S.TimeSliceSeconds;

	if (S.Stage == EStage::Enumerate)
	{
		if (!EnumerateCandidates(S, Deadline)) return false;
		if (S.Stage == EStage::Enumerate) S.Stage = EStage::Trace;
	}

	if (S.Stage == EStage::Trace)
	{
		if (!TraceCandidates(S, Deadline)) return false;
		S.Stage = EStage::Done;

		const int32 NumPoints = S.OutData ? S.OutData->GetPoints().Num() : 0;
		PCGE_LOG_C(Verbose, LogOnly, Context, FText::Format(
			LOCTEXT("GenerateReport", "Generated {0} points out of {1} candidate lattice points"),
			NumPoints, S.Candidates.Num()
		));
		UE_LOG(LogSplineImporter, Verbose, TEXT("OGR Surface Sampler: %d points out of %d candidates"), NumPoints, S.Candidates.Num());
	}

	return true;
#endif
}

#undef LOCTEXT_NAMESPACE
