// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "SplineImporter/GDALImporter.h"
#include "SplineImporter/LogSplineImporter.h"
#include "SplineImporter/Overpass.h"
#include "FileDownloader/Download.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "GDALInterface/GDALInterface.h"
#include "OSMUserData/OSMUserData.h"
#include "LCReporter/LCReporter.h"
#include "ConcurrencyHelpers/Concurrency.h"

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
#include "Misc/ScopedSlowTask.h"
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
		Source == EVectorSource::OSM_Parks;
}

void AGDALImporter::LoadGDALDatasetFromShortQuery(FString ShortQuery, bool bIsUserInitiated, TFunction<void(GDALDataset*)> OnComplete)
{
	UE_LOG(LogSplineImporter, Log, TEXT("Loading Dataset with Short Overpass Query: '%s'"), *ShortQuery);

	FVector4d Coordinates;

	if (BoundingMethod == EBoundingMethod::BoundingActor)
	{
		AActor *BoundingActor = nullptr;
		Concurrency::RunOnGameThreadAndWait([&BoundingActor, this]() { 
			BoundingActor = BoundingActorSelection.GetActor(this->GetWorld());
		});

		if (!IsValid(BoundingActor))
		{
			if (OnComplete) OnComplete(nullptr);
			return;
		}

		if (!BoundingActor->IsA<ALandscape>())
		{
			FVector Origin, BoxExtent;
			BoundingActor->GetActorBounds(true, Origin, BoxExtent);
			if (!ALevelCoordinates::GetCRSCoordinatesFromOriginExtent(BoundingActor->GetWorld(), Origin, BoxExtent, "EPSG:4326", Coordinates))
			{
				ULCReporter::ShowError(
					LOCTEXT("LoadGDALDatasetFromShortQuery2", "Internal error while reading coordinates. Make sure that your level coordinates are valid.")
				);
				if (OnComplete) OnComplete(nullptr);
				return;
			}
		}
		else
		{
			ALandscape *Landscape = Cast<ALandscape>(BoundingActor);

			if (!Concurrency::RunOnGameThreadAndReturn([Landscape, &Coordinates]() {
				return ALevelCoordinates::GetLandscapeCRSBounds(Landscape, "EPSG:4326", Coordinates);
			}))
			{
				if (OnComplete) OnComplete(nullptr);
				return;
			}
		}

		// EPSG 4326
		const double South = Coordinates[2];
		const double West = Coordinates[0];
		const double North = Coordinates[3];
		const double East = Coordinates[1];
		GDALInterface::LoadGDALVectorDatasetFromQuery(Overpass::QueryFromShortQuery(South, West, North, East, ShortQuery), bIsUserInitiated, OnComplete);
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

		GDALInterface::LoadGDALVectorDatasetFromQuery(Overpass::QueryFromShortQuery(South, West, North, East, ShortQuery), bIsUserInitiated, OnComplete);
	}
	else
	{
		check(false);
	}
}

void AGDALImporter::LoadGDALDataset(bool bIsUserInitiated, TFunction<void(GDALDataset*)> OnComplete)
{
	if (Source == EVectorSource::LocalFile)
	{
		if (OnComplete) OnComplete(GDALInterface::LoadGDALVectorDatasetFromFile(LocalFile));
	}
	else if (Source == EVectorSource::OverpassQuery)
	{
		GDALInterface::LoadGDALVectorDatasetFromQuery(
			FString("https://overpass-api.de/api/interpreter?data=") + OverpassQuery,
			bIsUserInitiated,
			OnComplete
		);
	}
	else if (Source == EVectorSource::OverpassShortQuery)
	{
		LoadGDALDatasetFromShortQuery(OverpassShortQuery, bIsUserInitiated, OnComplete);
	}
	else if (IsOverpassPreset())
	{
		SetOverpassShortQuery();
		LoadGDALDatasetFromShortQuery(OverpassShortQuery, bIsUserInitiated, OnComplete);
	}
	else
	{
		check(false);
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