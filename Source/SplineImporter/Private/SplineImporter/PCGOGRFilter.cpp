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
		if (UWorld* World = Context->ExecutionSource.IsValid() ? Context->ExecutionSource->GetExecutionState().GetWorld() : nullptr)
		{
			ThisContext->GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World, false);
		}
	}

	return true;
}

// adapted from Unreal Engine 5.2 PCGDensityFilter.cpp
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
		Outputs = Inputs;
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

	OGRGeometry *Geometry = Settings->GeometryActor->Geometry;
	if (!Geometry)
	{
		PCGE_LOG_C(Warning, GraphAndLog, Context,
			FText::Format(
				LOCTEXT("NoGeometry", "Unable to get OGR Geometry. Please make sure the OGRGeometry actor {0} is valid and initialized"),
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

		TMap<const FVector, FVector2D> PCGPointToPoint;
		TSet<FVector2D> InsideLocations;
		OGRMultiPoint *AllPoints = (OGRMultiPoint*) OGRGeometryFactory::createGeometry(OGRwkbGeometryType::wkbMultiPoint);
		
		if (!AllPoints)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("AllPointsNullPointer", "Couldn't create geometry"));
			return true;
		}

		UE_LOG(LogSplineImporter, Log, TEXT("Exploring %d PCG points"), PCGPoints.Num());
		
		for (const FPCGPoint& PCGPoint : PCGPoints)
		{
			const FVector& Location0 = PCGPoint.Transform.GetLocation();
			const FVector2D& Location = { Location0.X, Location0.Y };
			FVector2D Coordinates4326;

			if (!ThisContext->GlobalCoordinates->GetCRSCoordinatesFromUnrealLocation(Location, CoordinateTransformation, Coordinates4326))
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NoData", "Internal error, unable to convert coordinates, make sure that your LevelCoordinates actor has correct values"));
				return true;
			}
			OGRPoint Point4326(Coordinates4326[0], Coordinates4326[1]);
			PCGPointToPoint.Add(Location0, Coordinates4326);
			AllPoints->addGeometry(&Point4326);
		}
		UE_LOG(LogSplineImporter, Log, TEXT("Created All Points Geometry"));
		
		if (!AllPoints)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("AllPointsNullPointer", "Couldn't create geometry"));
			return true;
		}
		OGRGeometry *Intersection = AllPoints->Intersection(Geometry);
		
		if (!Intersection)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("IntersectionNullPointer", "Couldn't compute intersection"));
			return true;
		}

		if (wkbFlatten(Intersection->getGeometryType()) != wkbMultiPoint)
		{
			if (!Intersection->IsEmpty() && wkbFlatten(Intersection->getGeometryType()) != wkbPoint)
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(LOCTEXT("IntersectionNullPointer", "Unexpected error: Intersection is not a multi-point geometry ({0})"), { wkbFlatten(Intersection->getGeometryType()) }));
			}
			UE_LOG(LogSplineImporter, Log, TEXT("Empty Intersection"));
			return true;
		}

		OGRMultiPoint* IntersectionPoints = Intersection->toMultiPoint();
		
		if (!IntersectionPoints)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("IntersectionNotPoints", "Couldn't compute intersection as a set of points"));
			return true;
		}
		UE_LOG(LogSplineImporter, Log, TEXT("Valid Intersection Points with %d geometries"), IntersectionPoints->getNumGeometries());
		
		for (int i = 0, n = IntersectionPoints->getNumGeometries(); i < n; ++i)
		{
			OGRGeometry* PointGeometry = IntersectionPoints->getGeometryRef(i);
			if (!PointGeometry) continue;
			if (wkbFlatten(PointGeometry->getGeometryType()) != wkbPoint) continue;
			OGRPoint* Point = PointGeometry->toPoint();
			if (!Point) continue;
			InsideLocations.Add(FVector2D(Point->getX(), Point->getY()));
		}

		UE_LOG(LogSplineImporter, Log, TEXT("Starting AsyncPointProcessing, found %d locations inside"), InsideLocations.Num());

		
		FPCGAsync::AsyncPointProcessing(Context, PCGPoints.Num(), FilteredPoints,
			[InsideLocations, PCGPointToPoint, PCGPoints](int32 Index, FPCGPoint &OutPoint)
			{
				const FPCGPoint& PCGPoint = PCGPoints[Index];
				const FVector& Location = PCGPoint.Transform.GetLocation();
				if (InsideLocations.Contains(PCGPointToPoint[Location]))
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
