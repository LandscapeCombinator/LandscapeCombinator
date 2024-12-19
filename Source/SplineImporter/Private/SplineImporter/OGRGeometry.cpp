// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "SplineImporter/OGRGeometry.h"
#include "LCCommon/LCReporter.h"
#include "LCCommon/LCBlueprintLibrary.h"

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

	Super::SetOverpassShortQuery();
}

void AOGRGeometry::OnGenerate(FName SpawnedActorsPathOverride, TFunction<void(bool)> OnComplete)
{
	LoadGDALDataset([OnComplete, this](GDALDataset* Dataset)
	{
		if (!Dataset)
		{
			if (OnComplete) OnComplete(false);
			return;
		}

		UE_LOG(LogSplineImporter, Log, TEXT("Got a valid dataset to extract geometries, continuing..."));

		Geometry = OGRGeometryFactory::createGeometry(OGRwkbGeometryType::wkbMultiPolygon);
		if (!Geometry)
		{
			UE_LOG(LogSplineImporter, Error, TEXT("Internal error while creating OGR Geometry. Please try again."));
			if (OnComplete) OnComplete(false);
			return;
		}
		
		
		UWorld *World = this->GetWorld();
		TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
		OGRCoordinateTransformation *CoordinateTransformation = GDALInterface::MakeTransform("EPSG:4326", GlobalCoordinates->CRS);
		
		int n = Dataset->GetLayerCount();
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
				}
				else
				{
					UE_LOG(LogSplineImporter, Warning, TEXT("Error: %s"), *FString(CPLGetLastErrorMsg()));
					UE_LOG(LogSplineImporter, Warning, TEXT("There was an error while taking union of geometries in OGR, we'll still try to filter"))
				}
			}
		}

		this->Tags.AddUnique(AreaTag);
		
		if (OnComplete) OnComplete(true);
		return;
	});

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
		ULCReporter::ShowError(LOCTEXT("AOGRGeometry::DuplicateActor", "Failed to duplicate actor."));
		return nullptr;
	}
}

#endif

#undef LOCTEXT_NAMESPACE