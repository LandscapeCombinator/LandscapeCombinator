// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "SplineImporter/PCGOGRFilter.h"
#include "SplineImporter/LogSplineImporter.h"
#include "SplineImporter/Overpass.h"
#include "FileDownloader/Download.h"
#include "Coordinates/LevelCoordinates.h"

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

OGRGeometry* UPCGOGRFilterSettings::GetGeometryFromQuery(FString Query) const
{
	FString IntermediateDir = FPaths::ConvertRelativePathToFull(FPaths::EngineIntermediateDir());
	FString LandscapeCombinatorDir = FPaths::Combine(IntermediateDir, "LandscapeCombinator");
	FString DownloadDir = FPaths::Combine(LandscapeCombinatorDir, "Download");
	FString XmlFilePath = FPaths::Combine(DownloadDir,
		FString::Format(TEXT("overpass_query_{0}.xml"),
		{
			FTextLocalizationResource::HashString(Query)
		})
	);

	Download::SynchronousFromURL(Query, XmlFilePath);
	return GetGeometryFromPath(XmlFilePath);

}

OGRGeometry* UPCGOGRFilterSettings::GetGeometryFromShortQuery(UWorld *World, FBox Bounds, FString ShortQuery) const
{
	UE_LOG(LogSplineImporter, Log, TEXT("Resimulating foliage with short query: '%s'"), *ShortQuery);

	FVector4d Coordinates;
	if (!ALevelCoordinates::GetCRSCoordinatesFromFBox(World, Bounds, "EPSG:4326", Coordinates))
	{
		return nullptr;
	}

	double South = Coordinates[2];
	double North = Coordinates[3];
	double West = Coordinates[0];
	double East = Coordinates[1];

	FString OverpassQuery = Overpass::QueryFromShortQuery(South, West, North, East, ShortQuery);
	return GetGeometryFromQuery(OverpassQuery);
}

OGRGeometry* UPCGOGRFilterSettings::GetGeometryFromPath(FString Path) const
{
	GDALDataset* Dataset = (GDALDataset*) GDALOpenEx(TCHAR_TO_UTF8(*Path), GDAL_OF_VECTOR, NULL, NULL, NULL);

	if (!Dataset)
	{
		return nullptr;
	}

	UE_LOG(LogSplineImporter, Log, TEXT("Got a valid dataset to extract geometries, continuing..."));

	OGRGeometry *UnionGeometry = OGRGeometryFactory::createGeometry(OGRwkbGeometryType::wkbMultiPolygon);
	if (!UnionGeometry)
	{
		UE_LOG(LogSplineImporter, Error, TEXT("Internal error while creating OGR Geometry. Please try again."));
		return nullptr;
	}
		
	int n = Dataset->GetLayerCount();
	for (int i = 0; i < n; i++)
	{
		OGRLayer* Layer = Dataset->GetLayer(i);

		if (!Layer) continue;

		for (auto& Feature : Layer)
		{
			if (!Feature) continue;
			OGRGeometry* Geometry = Feature->GetGeometryRef();
			if (!Geometry) continue;

			OGRGeometry* NewUnion = UnionGeometry->Union(Geometry);
			if (NewUnion)
			{
				UnionGeometry = NewUnion;
			}
			else
			{
				UE_LOG(LogSplineImporter, Warning, TEXT("There was an error while taking union of geometries in OGR, we'll still try to filter"))
			}
		}

	}

	return UnionGeometry;
}

OGRGeometry* UPCGOGRFilterSettings::GetGeometry(UWorld *World, FBox Bounds) const
{
	
	if (FoliageSourceType == EFoliageSourceType::LocalVectorFile)
	{
		return GetGeometryFromPath(OSMPath);
	}
	else if (FoliageSourceType == EFoliageSourceType::OverpassShortQuery)
	{
		return GetGeometryFromShortQuery(World, Bounds, OverpassShortQuery);
	}
	else if (FoliageSourceType == EFoliageSourceType::Forests)
	{
		return GetGeometryFromShortQuery(World, Bounds, "nwr[\"landuse\"=\"forest\"];nwr[\"natural\"=\"wood\"];");
	}
	else
	{
		check(false);
		return nullptr;
	}
}

// adapted from Unreal Engine 5.2 PCGDensityFilter.cpp
bool FPCGOGRFilterElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGOGRFilterElement::Execute);

	const UPCGOGRFilterSettings* Settings = Context->GetInputSettings<UPCGOGRFilterSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	
	FBox Bounds = Cast<UPCGSpatialData>(Context->SourceComponent->GetActorPCGData())->GetBounds();

	OGRGeometry *Geometry = Settings->GetGeometry(Context->SourceComponent->GetWorld(), Bounds);

	if (!Geometry)
	{
		PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NoGeometry", "Unable to get OGR Geometry. Please check the Output Log"));
		return true;
	}


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

		for (const FPCGPoint& PCGPoint : PCGPoints)
		{
			const FVector& Location0 = PCGPoint.Transform.GetLocation();
			const FVector2D& Location = { Location0.X, Location0.Y };
			FVector2D Coordinates4326;

			if (!ALevelCoordinates::GetCRSCoordinatesFromUnrealLocation(Context->SourceComponent->GetWorld(), Location, "EPSG:4326", Coordinates4326))
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NoData", "Unable to convert coordinates, make sure that you have a LevelCoordinates actor in your level"));
				return false;
			}
			OGRPoint Point4326(Coordinates4326[0], Coordinates4326[1]);

			PCGPointToPoint.Add(Location0, Coordinates4326);

			AllPoints->addGeometry(&Point4326);
		}

		for (auto& Point : (OGRMultiPoint *) AllPoints->Intersection(Geometry))
		{
			InsideLocations.Add(FVector2D(Point->getX(), Point->getY()));
		}
		
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
