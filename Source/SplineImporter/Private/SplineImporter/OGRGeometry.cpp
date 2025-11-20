// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "SplineImporter/OGRGeometry.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OGRGeometry)

#define LOCTEXT_NAMESPACE "FSplineImporterModule"

void AOGRGeometry::SetOverpassShortQuery()
{
	if (Source == EVectorSource::OSM_Roads)
	{
		OverpassShortQuery = "way[\"highway\"][\"highway\"!~\"path\"][\"highway\"!~\"track\"];";
	}
	else if (Source == EVectorSource::OSM_Buildings)
	{
		OverpassShortQuery = "nwr[\"building\"];";
	}
	else if (Source == EVectorSource::OSM_Rivers)
	{
		OverpassShortQuery = "way[\"waterway\"=\"river\"];";
	}
	else if (Source == EVectorSource::OSM_Forests)
	{
		OverpassShortQuery = "nwr[\"landuse\"=\"forest\"];nwr[\"natural\"=\"wood\"];";
	}
	else if (Source == EVectorSource::OSM_Beaches)
	{
		OverpassShortQuery = "nwr[\"natural\"=\"beach\"];";
	}
	else if (Source == EVectorSource::OSM_Parks)
	{
		OverpassShortQuery = "nwr[\"leisure\"=\"park\"];";
	}
	else if (Source == EVectorSource::OSM_SkiSlopes)
	{
		OverpassShortQuery = "nwr[\"piste:type\"=\"downhill\"][\"area\"];";
	}
	else if (Source == EVectorSource::OSM_Grass)
	{
		OverpassShortQuery = "nwr[\"landuse\"=\"grass\"];nwr[\"natural\"=\"grassland\"];";
	}

	Super::SetOverpassShortQuery();
}

bool AOGRGeometry::OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated)
{
	Modify();
	
	GDALDataset* Dataset = LoadGDALDataset(bIsUserInitiated);
	if (!Dataset)
	{
		LCReporter::ShowError(LOCTEXT("AOGRGeometry::OnGenerate::NoDataset", "Could not load dataset for OGR Geometry."));
		return false;
	}

	UE_LOG(LogSplineImporter, Log, TEXT("Got a valid dataset to extract geometries, continuing..."));

	Geometry = OGRGeometryFactory::createGeometry(OGRwkbGeometryType::wkbMultiPolygon);
	if (!Geometry)
	{
		LCReporter::ShowError(LOCTEXT("AOGRGeometry::OnGenerate::NoGeometry", "Internal error while creating geometry, please try again."));
		return false;
	}
	
	int n = Dataset->GetLayerCount();
	int NumGeometries = 0;
	for (int i = 0; i < n; i++)
	{
		OGRLayer* Layer = Dataset->GetLayer(i);

		if (!Layer) continue;

		for (auto& Feature : Layer)
		{
			if (!Feature) continue;
			OGRGeometry* NewGeometry = Feature->GetGeometryRef();
			if (!NewGeometry) continue;

			if (!NewGeometry->IsValid())
			{
				UE_LOG(LogSplineImporter, Warning, TEXT("%s"), *FString(CPLGetLastErrorMsg()));
				UE_LOG(LogSplineImporter, Warning, TEXT("Obtained invalid geometry from feature, we'll make it valid and continue anyway"));
				NewGeometry = NewGeometry->MakeValid();
			}

			OGRGeometry* NewUnion = Geometry->Union(NewGeometry);
			if (NewUnion)
			{
				Geometry = NewUnion;
				NumGeometries++;
			}
			else
			{
				UE_LOG(LogSplineImporter, Warning, TEXT("Error: %s"), *FString(CPLGetLastErrorMsg()));
				UE_LOG(LogSplineImporter, Warning, TEXT("There was an error while taking union of geometries in OGR, we'll still skip a geometry"))
			}
		}
	}

	UE_LOG(LogSplineImporter, Log, TEXT("Found %d geometries"), NumGeometries);
	Tags.AddUnique(AreaTag);
	
	return true;
}

#if WITH_EDITOR

AActor *AOGRGeometry::Duplicate(FName FromName, FName ToName)
{
	if (AOGRGeometry *NewOGRGeometry =
		Cast<AOGRGeometry>(GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->DuplicateActor(this)))
	{
		NewOGRGeometry->BoundingActorSelection.ActorTag =
			ULCBlueprintLibrary::ReplaceName(
				BoundingActorSelection.ActorTag,
				FromName,
				ToName
			);
		NewOGRGeometry->AreaTag = ULCBlueprintLibrary::ReplaceName(AreaTag, FromName, ToName);

		return NewOGRGeometry;
	}
	else
	{
		LCReporter::ShowError(LOCTEXT("AOGRGeometry::DuplicateActor", "Failed to duplicate actor."));
		return nullptr;
	}
}

#endif

#undef LOCTEXT_NAMESPACE