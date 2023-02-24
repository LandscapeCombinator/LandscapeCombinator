#include "Foliage/OGRProceduralFoliageVolume.h"
#include "GlobalSettings.h"
#include "Utils/Logging.h"
#include "Utils/Download.h"
#include "Utils/Time.h"
#include "Utils/Overpass.h"
#include "Elevation/HeightMapTable.h"

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include <ogr_api.h>
#include <ogrsf_frmts.h>
#pragma warning(default: 4668)

#include "ProceduralFoliageComponent.h"
#include "Editor/FoliageEdit/Private/FoliageEdMode.h"
#include "Internationalization/TextLocalizationResource.h"




#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

using namespace GlobalSettings;

void AOGRProceduralFoliageVolume::ResimulateWithFilterFromShortQuery(const FString& ShortQuery)
{
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Resimulating foliage with short query: '%s'"), *ShortQuery);

	FVector Origin;
	FVector BoxExtent;
	GetActorBounds(false, Origin, BoxExtent, true);
	
	WorldParametersV1 GlobalParams;
	if (!GetWorldParameters(GlobalParams)) return;
	
	double WorldWidthCm  = ((double) GlobalParams.WorldWidthKm) * 1000 * 100;
	double WorldHeightCm = ((double) GlobalParams.WorldHeightKm) * 1000 * 100;
	double WorldOriginX = GlobalParams.WorldOriginX;
	double WorldOriginY = GlobalParams.WorldOriginY;
	
	double South = UnrealCoordinatesToEPSG326Y(Origin.Y + BoxExtent.Y, WorldWidthCm, WorldHeightCm, WorldOriginX, WorldOriginY);
	double North = UnrealCoordinatesToEPSG326Y(Origin.Y - BoxExtent.Y, WorldWidthCm, WorldHeightCm, WorldOriginX, WorldOriginY);
	double West = UnrealCoordinatesToEPSG326X(Origin.X - BoxExtent.X, WorldWidthCm, WorldHeightCm, WorldOriginX, WorldOriginY);
	double East = UnrealCoordinatesToEPSG326X(Origin.X + BoxExtent.X, WorldWidthCm, WorldHeightCm, WorldOriginX, WorldOriginY);

	FString OverpassQuery = Overpass::QueryFromShortQuery(South, West, North, East, ShortQuery);
	ResimulateWithFilterFromQuery(OverpassQuery);
}

void AOGRProceduralFoliageVolume::ResimulateWithFilterFromQuery(const FString& OverpassQuery)
{
	FString IntermediateDir = FPaths::ConvertRelativePathToFull(FPaths::EngineIntermediateDir());
	FString LandscapeCombinatorDir = FPaths::Combine(IntermediateDir, "LandscapeCombinator");
	FString DownloadDir = FPaths::Combine(LandscapeCombinatorDir, "Download");
	FString XmlFilePath = FPaths::Combine(DownloadDir,
		FString::Format(TEXT("overpass_query_{0}.xml"),
		{
			FTextLocalizationResource::HashString(OverpassQuery)
		})
	);

	Download::FromURL(OverpassQuery, XmlFilePath,
		[this, XmlFilePath, OverpassQuery](bool bWasSuccessful) {
			if (bWasSuccessful)
			{
				ResimulateWithFilterFromFile(XmlFilePath);
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
					LOCTEXT("GetSpatialReferenceError", "Unable to get the result for the Overpass query: {0}."),
					FText::FromString(OverpassQuery)
				));
			}
			return;
		}
	);
}

void AOGRProceduralFoliageVolume::ResimulateWithFilterFromFile(const FString& Path)
{
	if (!ProceduralComponent)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("NoProceduralComponent", "This Procedural Foliage Volume does not have a Procedural Foliage Component.")
		);
		return;
	}
	Time::TimeSpent.Empty();

	GDALDataset* Dataset = (GDALDataset*) GDALOpenEx(TCHAR_TO_UTF8(*Path), GDAL_OF_VECTOR, NULL, NULL, NULL);

	if (!Dataset)
	{
		return;
	}

	UE_LOG(LogLandscapeCombinator, Log, TEXT("Got a valid dataset from OSM data, continuing..."));


	OGRGeometry *UnionGeometry = OGRGeometryFactory::createGeometry(OGRwkbGeometryType::wkbMultiPolygon);
	if (!UnionGeometry)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("UnionGeometryError", "Internal error while creating OGR Geometry. Please try again."));
		return;
	}
	
	int n = Dataset->GetLayerCount();
	for (int i = 0; i < n; i++)
	{
		OGRLayer* Layer = Dataset->GetLayer(i);

		if (!Layer) continue;

		for (auto& Feature : Layer)
		{			
			OGRGeometry* Geometry = Time::Time<OGRGeometry*>(FString("GetGeometryRef"), [&]() { return Feature->GetGeometryRef(); });
			if (!Geometry) continue;
			UnionGeometry = UnionGeometry->Union(Geometry);
		}

	}
	
	ProceduralComponent->ResimulateProceduralFoliage(
		[this, &Dataset, &UnionGeometry](const TArray<FDesiredFoliageInstance>& DesiredFoliageInstances) {
			FFoliagePaintingGeometryFilter OverrideGeometryFilter;
			OverrideGeometryFilter.bAllowLandscape = ProceduralComponent->bAllowLandscape;
			OverrideGeometryFilter.bAllowStaticMesh = ProceduralComponent->bAllowStaticMesh;
			OverrideGeometryFilter.bAllowBSP = ProceduralComponent->bAllowBSP;
			OverrideGeometryFilter.bAllowFoliage = ProceduralComponent->bAllowFoliage;
			OverrideGeometryFilter.bAllowTranslucent = ProceduralComponent->bAllowTranslucent;
			
			WorldParametersV1 GlobalParams;
			if (!GetWorldParameters(GlobalParams)) return;

			double WorldWidthCm  = ((double) GlobalParams.WorldWidthKm) * 1000 * 100;
			double WorldHeightCm = ((double) GlobalParams.WorldHeightKm) * 1000 * 100;
			double WorldOriginX = GlobalParams.WorldOriginX;
			double WorldOriginY = GlobalParams.WorldOriginY;


			UE_LOG(LogLandscapeCombinator, Log, TEXT("Resimulating foliage with %d desired foliage instances"), DesiredFoliageInstances.Num());
			
			OGRMultiPoint *AllPoints = (OGRMultiPoint*) OGRGeometryFactory::createGeometry(OGRwkbGeometryType::wkbMultiPoint);

			TMap<FVector2D, const FDesiredFoliageInstance> PointToInstance;
			for (auto& DesiredFoliageInstance : DesiredFoliageInstances)
			{
				FVector Location = DesiredFoliageInstance.StartTrace;
				FVector2D Coordinates4326 = GlobalSettings::UnrealCoordinatesToEPSG326(Location, WorldWidthCm, WorldHeightCm, WorldOriginX, WorldOriginY);

				OGRPoint Point(Coordinates4326[0], Coordinates4326[1]);
				PointToInstance.Add(FVector2D(Coordinates4326[0], Coordinates4326[1]), DesiredFoliageInstance);

				AllPoints->addGeometry(&Point);
			}
			
			TArray<FDesiredFoliageInstance> FilteredFoliageInstances;
			for (auto& Point : (OGRMultiPoint *) AllPoints->Intersection(UnionGeometry))
			{
				FilteredFoliageInstances.Add(PointToInstance[FVector2D(Point->getX(), Point->getY())]);
			}
			
			UE_LOG(LogLandscapeCombinator, Log, TEXT("After filtering with OSM data, the geometry contains %d foliage instances"), FilteredFoliageInstances.Num());
			FEdModeFoliage::AddInstances(ProceduralComponent->GetWorld(), FilteredFoliageInstances, OverrideGeometryFilter, true);
		}
	);
}

void AOGRProceduralFoliageVolume::ResimulateWithFilter()
{
	if (FoliageSourceType == EFoliageSourceType::OverpassShortQuery || FoliageSourceType == EFoliageSourceType::Forests)
	{
		EAppReturnType::Type UserResponse = FMessageDialog::Open(EAppMsgType::OkCancel,
			LOCTEXT("ResimulateWithFilterMessage",
				"We will do an Overpass Query to find the regions in which to place Procedural Foliage.\n"
				"You can monitor the download progress in the Output Log in the LogLandscapeCombinator category."
			)
		);
		if (UserResponse == EAppReturnType::Cancel)
		{
			return;
		}
	}

	if (FoliageSourceType == EFoliageSourceType::LocalVectorFile)
	{
		ResimulateWithFilterFromFile(OSMPath);
	}
	else if (FoliageSourceType == EFoliageSourceType::OverpassShortQuery)
	{
		ResimulateWithFilterFromShortQuery(OverpassShortQuery);
	}
	else if (FoliageSourceType == EFoliageSourceType::Forests)
	{
		ResimulateWithFilterFromShortQuery("nwr[\"natural\"=\"wood\"];nwr[\"landuse\"=\"forest\"];");
	}
}

#undef LOCTEXT_NAMESPACE