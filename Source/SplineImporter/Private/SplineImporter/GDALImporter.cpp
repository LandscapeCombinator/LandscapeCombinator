// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "SplineImporter/GDALImporter.h"
#include "SplineImporter/LogSplineImporter.h"
#include "SplineImporter/Overpass.h"
#include "FileDownloader/Download.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "GDALInterface/GDALInterface.h"
#include "OSMUserData/OSMUserData.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "LandscapeSplineActor.h"
#include "ILandscapeSplineInterface.h"
#include "EditorSupportDelegates.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeSplineControlPoint.h" 
#include "LandscapeSplinesComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/SWindow.h"
#include "Misc/CString.h"
#include "Logging/StructuredLog.h"
#include "Async/Async.h"
#include "Stats/Stats.h"
#include "Stats/StatsMisc.h"
#include "ObjectEditorUtils.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

AGDALImporter::AGDALImporter()
{
	PrimaryActorTick.bCanEverTick = false;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent")));
	RootComponent->SetMobility(EComponentMobility::Static);

	PositionBasedGeneration = CreateDefaultSubobject<ULCPositionBasedGeneration>(TEXT("PositionBasedGeneration"));

	SetOverpassShortQuery();
}

void AGDALImporter::SetOverpassShortQuery()
{
	if (IsOverpassPreset()) OverpassShortQueryPreset = OverpassShortQuery;
}

bool AGDALImporter::IsOverpassPreset()
{
	return
		Source == EVectorSource::OSM_Roads ||
		Source == EVectorSource::OSM_Buildings ||
		Source == EVectorSource::OSM_Rivers ||
		Source == EVectorSource::OSM_Forests ||
		Source == EVectorSource::OSM_Beaches ||
		Source == EVectorSource::OSM_Parks ||
		Source == EVectorSource::OSM_SkiSlopes ||
		Source == EVectorSource::OSM_Grass;
}

bool AGDALImporter::IsOverpass()
{
	return IsOverpassPreset() ||
		Source == EVectorSource::OverpassQuery ||
		Source == EVectorSource::OverpassShortQuery;
}

GDALDataset* AGDALImporter::LoadGDALDatasetFromShortQuery(FString ShortQuery, bool bIsUserInitiated)
{
	UE_LOG(LogSplineImporter, Log, TEXT("Loading Dataset with Short Overpass Query: '%s'"), *ShortQuery);

	FVector4d Coordinates;

	if (BoundingMethod == EBoundingMethod::BoundingActor)
	{
		AActor *BoundingActor = nullptr;
		Concurrency::RunOnGameThreadAndWait([&BoundingActor, this]() { 
			BoundingActor = BoundingActorSelection.GetActor(this->GetWorld());
			return true;
		});

		if (!IsValid(BoundingActor)) return nullptr;

		if (!BoundingActor->IsA<ALandscape>())
		{
			FVector Origin, BoxExtent;
			BoundingActor->GetActorBounds(true, Origin, BoxExtent);
			if (!ALevelCoordinates::GetCRSCoordinatesFromOriginExtent(BoundingActor->GetWorld(), Origin, BoxExtent, "EPSG:4326", Coordinates))
			{
				LCReporter::ShowError(
					LOCTEXT("LoadGDALDatasetFromShortQuery2", "Internal error while reading coordinates. Make sure that your level coordinates are valid.")
				);
				return nullptr;
			}
		}
		else
		{
			ALandscape *Landscape = Cast<ALandscape>(BoundingActor);

			if (!Concurrency::RunOnGameThreadAndWait([Landscape, &Coordinates]() {
				return LandscapeUtils::GetLandscapeCRSBounds(Landscape, "EPSG:4326", Coordinates);
			}))
			{
				return nullptr;
			}
		}

		// EPSG 4326
		const double South = Coordinates[2];
		const double West = Coordinates[0];
		const double North = Coordinates[3];
		const double East = Coordinates[1];
		return GDALInterface::LoadGDALVectorDatasetFromQuery(Overpass::QueryFromShortQuery(OverpassServer, South, West, North, East, ShortQuery), bIsUserInitiated);
	}
	else if (BoundingMethod == EBoundingMethod::TileNumbers)
	{
		const double n = 1 << BoundingZoneZoom;
		const double West = BoundingZoneMinX / n * 360.0 - 180;
		const double East = (BoundingZoneMaxX + 1) / n * 360.0 - 180;
		const double NorthRad = FMath::Atan(FMath::Sinh(UE_PI * (1 - 2 * BoundingZoneMinY / n)));
		const double SouthRad = FMath::Atan(FMath::Sinh(UE_PI * (1 - 2 * (BoundingZoneMaxY + 1) / n)));
		const double North = FMath::RadiansToDegrees(NorthRad);
		const double South = FMath::RadiansToDegrees(SouthRad);

		return GDALInterface::LoadGDALVectorDatasetFromQuery(Overpass::QueryFromShortQuery(OverpassServer, South, West, North, East, ShortQuery), bIsUserInitiated);
	}
	else
	{
		check(false);
		return nullptr;
	}
}

GDALDataset* AGDALImporter::LoadGDALDataset(bool bIsUserInitiated)
{
	if (Source == EVectorSource::LocalFile) return GDALInterface::LoadGDALVectorDatasetFromFile(LocalFile);
	else if (Source == EVectorSource::OverpassQuery)
	{
		return GDALInterface::LoadGDALVectorDatasetFromQuery(
			OverpassServer + "?data=" + OverpassQuery,
			bIsUserInitiated
		);
	}
	else if (Source == EVectorSource::OverpassShortQuery)
	{
		return LoadGDALDatasetFromShortQuery(OverpassShortQuery, bIsUserInitiated);
	}
	else if (IsOverpassPreset())
	{
		SetOverpassShortQuery();
		return LoadGDALDatasetFromShortQuery(OverpassShortQuery, bIsUserInitiated);
	}
	else
	{
		check(false);
		return nullptr;
	}
}

#if WITH_EDITOR

void AGDALImporter::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	if (!Event.Property)
	{
		Super::PostEditChangeProperty(Event);
		return;
	}

	FName PropertyName = Event.Property->GetFName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AGDALImporter, Source))
	{
		SetOverpassShortQuery();
	}

	Super::PostEditChangeProperty(Event);
}

#endif

#undef LOCTEXT_NAMESPACE
