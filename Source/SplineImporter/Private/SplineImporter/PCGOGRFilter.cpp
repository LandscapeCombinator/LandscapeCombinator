// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "SplineImporter/PCGOGRFilter.h"
#include "SplineImporter/LogSplineImporter.h"
#include "SplineImporter/Overpass.h"
#include "FileDownloader/Download.h"
#include "Coordinates/LevelCoordinates.h"

#include "Kismet/GameplayStatics.h"
#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "PCGParamData.h"
#include "PCGElement.h"
#include "PCGComponent.h"
#include "Elements/PCGSurfaceSampler.h"
#include "PCGPin.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Metadata/Accessors/IPCGAttributeAccessorTpl.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGCustomAccessor.h"
#include "Engine/World.h"
#include "HAL/ThreadManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGOGRFilter)

namespace
{
// true = inside or on the boundary (holes respected). No GEOS needed.
bool IntersectsAreal(const OGRGeometry* G, const OGRPoint& P)
{
if (!G || G->IsEmpty()) return false;
switch (wkbFlatten(G->getGeometryType()))
{
case wkbPolygon:
{
const OGRPolygon* Poly = G->toPolygon();
const OGRLinearRing* Ext = Poly->getExteriorRing();
if (!Ext || Ext->IsEmpty()) return false;
if (!Ext->isPointInRing(&P) && !Ext->isPointOnRingBoundary(&P)) return false;
for (int i = 0, n = Poly->getNumInteriorRings(); i < n; ++i)
{
const OGRLinearRing* Hole = Poly->getInteriorRing(i);
if (Hole && !Hole->IsEmpty() && Hole->isPointInRing(&P) && !Hole->isPointOnRingBoundary(&P)) return false;
}
return true;
}
case wkbCurvePolygon:
case wkbMultiSurface:
{
OGRGeometry* Linear = G->getLinearGeometry();
if (!Linear) return false;
const bool bResult = IntersectsAreal(Linear, P);
OGRGeometryFactory::destroyGeometry(Linear);
return bResult;
}
case wkbMultiPolygon:
case wkbGeometryCollection:
{
const OGRGeometryCollection* C = G->toGeometryCollection();
for (int i = 0, n = C->getNumGeometries(); i < n; ++i)
{
if (IntersectsAreal(C->getGeometryRef(i), P)) return true;
}
return false;
}
default:
return false;
}
}
}

#define LOCTEXT_NAMESPACE "FSplineImporterModule"

FPCGElementPtr UPCGOGRFilterSettings::CreateElement() const
{
	return MakeShared<FPCGOGRFilterElement>();
}

bool FPCGOGRFilterElement::PrepareDataInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGOGRFilterElement::PrepareDataInternal);

	FPCGOGRFilterContext* ThisContext = static_cast<FPCGOGRFilterContext*>(Context);

	if (ThisContext && !IsValid(ThisContext->GlobalCoordinates))
	{
		UWorld* World = nullptr;

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
		if (Context && Context->ExecutionSource.IsValid())
		{
			World = Context->ExecutionSource->GetExecutionState().GetWorld();
		}

#else
		if (Context && Context->SourceComponent.IsValid())
		{
			World = Context->SourceComponent->GetWorld();
		}
#endif

		if (IsValid(World)) ThisContext->GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World, false);
	}

	return true;
}

bool FPCGOGRFilterElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGOGRFilterElement::ExecuteInternal);

	const UPCGOGRFilterSettings* Settings = Context->GetInputSettings<UPCGOGRFilterSettings>();
	check(Settings);
	
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetAllSpatialInputs();
#else
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
#endif

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	if (!IsValid(Settings->GeometryActor))
	{
		PCGE_LOG_C(Warning, GraphAndLog, Context,
			LOCTEXT("NoGeometry", "Unable to get OGR Geometry. Please make sure the OGRGeometry actor is valid")
		);
		return true;
	}

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
	UPCGComponent* PCGComponent = Cast<UPCGComponent>(Context->ExecutionSource.Get());
	if (!IsValid(PCGComponent))
#else
	TWeakObjectPtr<UPCGComponent> PCGComponent = Context->SourceComponent;
	if (!PCGComponent.IsValid())
#endif
	{
		PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NoPCGComponent", "Unable to get source PCG Component."));
		return true;
	}

	FPCGOGRFilterContext* ThisContext = static_cast<FPCGOGRFilterContext*>(Context);
	if (!ThisContext)
	{
		PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("InvalidContext", "Internal Error: Invalid FPCGOGRFilterContext."));
		return true;
	}

	OGRGeometry *Geometry = Settings->GeometryActor->CloneGeometry();

	ON_SCOPE_EXIT { 
		if (Geometry) OGRGeometryFactory::destroyGeometry(Geometry); 
	};
	if (!Geometry)
	{
		PCGE_LOG_C(Warning, GraphAndLog, Context,
			FText::Format(
				LOCTEXT("NoGeometry", "Unable to get OGR Geometry. Please make sure the OGRGeometry actor {0} is initialized"),
				FText::FromString(Settings->GeometryActor->GetActorNameOrLabel())
			)
		);
		return true;
	}

	OGRCoordinateTransformation *CoordinateTransformation = GDALInterface::MakeTransform(ThisContext->GlobalCoordinates->CRS, "EPSG:4326");

	for (const FPCGTaggedData& Input : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
	{
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);

		if (!Input.Data || Cast<UPCGSpatialData>(Input.Data) == nullptr)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("InvalidInputData", "Invalid input data"));
			continue;
		}

		const UPCGPointData* OriginalData = Cast<UPCGSpatialData>(Input.Data)->ToPointData(Context);

		if (!OriginalData)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NoData", "Unable to get point data from input"));
			continue;
		}

		const TArray<FPCGPoint>& PCGPoints = OriginalData->GetPoints();
		
		UPCGPointData* FilteredData = NewObject<UPCGPointData>();
		FilteredData->InitializeFromData(OriginalData);
		TArray<FPCGPoint>& FilteredPoints = FilteredData->GetMutablePoints();

		Output.Data = FilteredData;

TArray<bool> bInside;
bInside.Init(false, PCGPoints.Num());
int32 NumInside = 0;

UE_LOG(LogSplineImporter, Log, TEXT("Exploring %d PCG points"), PCGPoints.Num());

for (int32 i = 0; i < PCGPoints.Num(); ++i)
{
const FVector Location0 = PCGPoints[i].Transform.GetLocation();
const FVector2D Location = { Location0.X, Location0.Y };
FVector2D Coordinates4326;

if (!ThisContext->GlobalCoordinates->GetCRSCoordinatesFromUnrealLocation(Location, CoordinateTransformation, Coordinates4326))
{
PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NoData", "Internal error, unable to convert coordinates, make sure that your LevelCoordinates actor has correct values"));
return true;
}
OGRPoint Point4326(Coordinates4326[0], Coordinates4326[1]);
if (IntersectsAreal(Geometry, Point4326))
{
bInside[i] = true;
++NumInside;
}
}

UE_LOG(LogSplineImporter, Log, TEXT("Starting AsyncPointProcessing, found %d locations inside"), NumInside);

		
		FPCGAsync::AsyncPointProcessing(Context, PCGPoints.Num(), FilteredPoints,
			[bInside = MoveTemp(bInside), PCGPoints](int32 Index, FPCGPoint &OutPoint)
			{
				const FPCGPoint& PCGPoint = PCGPoints[Index];
				if (bInside[Index])
				{
					OutPoint = PCGPoint;
					return true;
				}
				else
				{
					return false;
				}
			}
		);

		PCGE_LOG_C(Verbose, LogOnly, Context, FText::Format(
			LOCTEXT("GenerateReport", "Generated {0} points out of {1} source points"),
			FilteredPoints.Num(),
			PCGPoints.Num()
		));
		UE_LOG(LogSplineImporter, Log, TEXT("Generated %d filtered points out of %d source points"),
			FilteredPoints.Num(),
			PCGPoints.Num());
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
