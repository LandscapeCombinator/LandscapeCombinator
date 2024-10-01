// Copyright 2023 LandscapeCombinator. All Rights Reserved.

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


#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGOGRFilter)

#define LOCTEXT_NAMESPACE "FSplineImporterModule"

FPCGElementPtr UPCGOGRFilterSettings::CreateElement() const
{
	return MakeShared<FPCGOGRFilterElement>();
}


// adapted from Unreal Engine 5.2 PCGDensityFilter.cpp
bool FPCGOGRFilterElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGOGRFilterElement::Execute);

	const UPCGOGRFilterSettings* Settings = Context->GetInputSettings<UPCGOGRFilterSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	TArray<AActor*> Geometries;
	UGameplayStatics::GetAllActorsOfClassWithTag(Cast<UObject>(Context->SourceComponent->GetWorld()), AOGRGeometry::StaticClass(), Settings->GeometryTag, Geometries);

	if (Geometries.Num() == 0)
	{
		PCGE_LOG_C(
			Error, GraphAndLog, Context,
			FText::Format(
				LOCTEXT("NoGeometry", "Found no OGRGeometry actor with tag {0}."),
				FText::FromName(Settings->GeometryTag)
			)
		);
		return true;
	}

	AOGRGeometry *GeometryActor = Cast<AOGRGeometry>(Geometries[0]);

	OGRGeometry *Geometry = IsValid(GeometryActor) ? GeometryActor->Geometry : nullptr;

	if (!Geometry)
	{
		PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NoGeometry", "Unable to get OGR Geometry. Please make sure the OGRGeometry actor is valid and initialized"));
		return true;
	}

	UWorld *World = Context->SourceComponent->GetWorld();
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates)
	{
		PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NoGlobalCoordinates", "No Global Coordinates"));
		return true;
	}

	OGRCoordinateTransformation *CoordinateTransformation = GDALInterface::MakeTransform(GlobalCoordinates->CRS, "EPSG:4326");

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
		
		UE_LOG(LogSplineImporter, Log, TEXT("Exploring %d PCG points"), PCGPoints.Num());
		
		FlushPersistentDebugLines(World);
		for (const FPCGPoint& PCGPoint : PCGPoints)
		{
			const FVector& Location0 = PCGPoint.Transform.GetLocation();
			const FVector2D& Location = { Location0.X, Location0.Y };
			FVector2D Coordinates4326;

			if (!GlobalCoordinates->GetCRSCoordinatesFromUnrealLocation(Location, CoordinateTransformation, Coordinates4326))
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NoData", "Internal error, unable to convert coordinates, make sure that your LevelCoordinates actor has correct values"));
				return true;
			}
			OGRPoint Point4326(Coordinates4326[0], Coordinates4326[1]);

			PCGPointToPoint.Add(Location0, Coordinates4326);
			AllPoints->addGeometry(&Point4326);
		}
		
		for (auto& Point : (OGRMultiPoint *) AllPoints->Intersection(Geometry))
		{
			InsideLocations.Add(FVector2D(Point->getX(), Point->getY()));
		}

		UE_LOG(LogSplineImporter, Log, TEXT("Starting AsyncPointProcessing, found %d locations inside"), InsideLocations.Num());

		
		FPCGAsync::AsyncPointProcessing(Context, PCGPoints.Num(), FilteredPoints,
			[InsideLocations, PCGPointToPoint, PCGPoints, World](int32 Index, FPCGPoint &OutPoint)
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
