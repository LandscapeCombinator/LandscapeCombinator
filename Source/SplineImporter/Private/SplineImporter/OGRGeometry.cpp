// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "SplineImporter/OGRGeometry.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "ConcurrencyHelpers/Concurrency.h"

#include "Components/DecalComponent.h"
#include "Coordinates/DecalCoordinates.h"
#include "Coordinates/GlobalCoordinates.h"
#include "Coordinates/LevelCoordinates.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OGRGeometry)

#define LOCTEXT_NAMESPACE "FSplineImporterModule"

AOGRGeometry::AOGRGeometry()
{
	PreviewDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("PreviewDecal"));
	PreviewDecal->SetupAttachment(RootComponent);
	PreviewDecal->SetVisibility(false);
}

void AOGRGeometry::SetOverpassShortQuery()
{
	if (Source == EVectorSource::OSM_Roads)
	{
		OverpassShortQuery = "way[\"highway\"][\"highway\"!~\"path\"][\"highway\"!~\"track\"][\"highway\"!~\"footway\"][\"service\"!~\"driveway\"];";
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
	TWeakObjectPtr<AOGRGeometry> WeakThis(this);

	if (!Concurrency::RunOnGameThreadAndWait([WeakThis]() {
		if (!WeakThis.IsValid()) return false;
		WeakThis->Modify();
		return true;
	}))
	{
		UE_LOG(LogSplineImporter, Warning, TEXT("OnGenerate: actor no longer valid before Modify() could run -- aborting"));
		return false;
	}

	GDALDataset* Dataset = LoadGDALDataset(bIsUserInitiated);
	if (!Dataset)
	{
		if (bIsUserInitiated) LCReporter::ShowError(LOCTEXT("AOGRGeometry::OnGenerate::NoDataset", "Could not load dataset for OGR Geometry."));
		return false;
	}

	UE_LOG(LogSplineImporter, Log, TEXT("Got a valid dataset to extract geometries, continuing..."));

	if (bClearGeometryBeforeImporting || !Geometry)
	{
		FScopeLock Lock(&GeometryLock);
		if (Geometry) OGRGeometryFactory::destroyGeometry(Geometry);
		Geometry = OGRGeometryFactory::createGeometry(OGRwkbGeometryType::wkbMultiPolygon);
	}

	if (!Geometry)
	{
		LCReporter::ShowError(LOCTEXT("AOGRGeometry::OnGenerate::NoGeometry", "Internal error while creating geometry, please try again."));
		return false;
	}
	
	int n = Dataset->GetLayerCount();
	int NumGeometries = 0;
	bool bAnyUnion = false;

	for (int i = 0; i < n; i++)
	{
		OGRLayer* Layer = Dataset->GetLayer(i);

		if (!Layer) continue;

		for (auto& Feature : Layer)
		{
			if (!Feature) continue;

			if (!GDALInterface::AddFeature(AlreadyHandledFeatures, Feature.get())) continue;

			OGRGeometry* NewGeometry = Feature->GetGeometryRef();
			if (!NewGeometry) continue;

			OGRGeometry* MadeValidGeometry = nullptr;
			if (!NewGeometry->IsValid())
			{
				UE_LOG(LogSplineImporter, Warning, TEXT("%s"), *FString(CPLGetLastErrorMsg()));
				UE_LOG(LogSplineImporter, Warning, TEXT("Obtained invalid geometry from feature, we'll make it valid and continue anyway"));
				MadeValidGeometry = NewGeometry->MakeValid();
				NewGeometry = MadeValidGeometry;
				if (!NewGeometry)
				{
					UE_LOG(LogSplineImporter, Warning, TEXT("MakeValid() returned null, skipping this feature"));
					continue;
				}
			}

			OGRGeometry* NewUnion = Geometry->Union(NewGeometry);
			if (MadeValidGeometry) OGRGeometryFactory::destroyGeometry(MadeValidGeometry);

			if (NewUnion)
			{
				{
					FScopeLock Lock(&GeometryLock);
					OGRGeometryFactory::destroyGeometry(Geometry);
					Geometry = NewUnion;
				}
				NumGeometries++;
				bAnyUnion = true;
			}
			else
			{
				UE_LOG(LogSplineImporter, Warning, TEXT("Error: %s"), *FString(CPLGetLastErrorMsg()));
				UE_LOG(LogSplineImporter, Warning, TEXT("There was an error while taking union of geometries in OGR, we'll skip a geometry"))
			}
		}
	}

	if (bAnyUnion) FlushPCGCacheIfNeeded();

	UE_LOG(LogSplineImporter, Log, TEXT("Found %d geometries"), NumGeometries);
	Tags.AddUnique(AreaTag);
	SetGeometry(Geometry);

	return true;
}

void AOGRGeometry::SetGeometry(OGRGeometry* NewGeometry)
{
	{
		FScopeLock Lock(&GeometryLock);
		if (Geometry && Geometry != NewGeometry)
		{
			OGRGeometryFactory::destroyGeometry(Geometry);
		}
		Geometry = NewGeometry;
	}

	SerializedGeometry.Reset();
	if (Geometry)
	{
		const int32 WkbSize = static_cast<int32>(Geometry->WkbSize());
		if (WkbSize > 0)
		{
			SerializedGeometry.SetNumUninitialized(WkbSize);
			if (Geometry->exportToWkb(SerializedGeometry.GetData()) != OGRERR_NONE)
			{
				UE_LOG(LogSplineImporter, Error, TEXT("SetGeometry: exportToWkb failed — geometry will NOT survive a restart/repackage until the next successful update"));
				SerializedGeometry.Reset();
			}
		}
	}

	UpdateGeometryPreview();
}

void AOGRGeometry::RebuildGeometryFromSerialized()
{
	if (Geometry) return;
	if (SerializedGeometry.Num() == 0) return;

	OGRGeometry* NewGeometry = nullptr;
	OGRErr Err = OGRGeometryFactory::createFromWkb(
		SerializedGeometry.GetData(),
		nullptr,
		&NewGeometry,
		SerializedGeometry.Num()
	);

	if (Err != OGRERR_NONE || !NewGeometry)
	{
		UE_LOG(LogSplineImporter, Error, TEXT("RebuildGeometryFromSerialized: failed to reconstruct geometry from %d bytes of serialized WKB (OGRErr=%d) — preview will stay hidden until the next Import"), SerializedGeometry.Num(), (int)Err);
		return;
	}

	Geometry = NewGeometry;
}

void AOGRGeometry::UpdateGeometryPreview()
{
	OGRGeometry* ProjectedGeometry = CloneGeometry();

	if (!bShowGeometryPreview || !ProjectedGeometry || !IsValid(PreviewMaterial))
	{
		if (ProjectedGeometry) OGRGeometryFactory::destroyGeometry(ProjectedGeometry);
		HideGeometryPreview();
		return;
	}

	UGlobalCoordinates* GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(GetWorld());
	if (!IsValid(GlobalCoordinates))
	{
		OGRGeometryFactory::destroyGeometry(ProjectedGeometry);
		HideGeometryPreview();
		return;
	}
	UE_LOG(LogSplineImporter, Log, TEXT("UpdateGeometryPreview: using GlobalCoordinates CRS='%s', CmPerLongUnit=%f, CmPerLatUnit=%f, WorldOriginLong=%f, WorldOriginLat=%f"),
		*GlobalCoordinates->CRS, GlobalCoordinates->CmPerLongUnit, GlobalCoordinates->CmPerLatUnit,
		GlobalCoordinates->WorldOriginLong, GlobalCoordinates->WorldOriginLat);
	
	OGRCoordinateTransformation* Transform = GlobalCoordinates->GetCRSTransformer("EPSG:4326");
	if (!Transform)
	{
		OGRGeometryFactory::destroyGeometry(ProjectedGeometry);
		HideGeometryPreview();
		return;
	}

	if (ProjectedGeometry->transform(Transform) != OGRERR_NONE)
	{
		UE_LOG(LogSplineImporter, Warning, TEXT("UpdateGeometryPreview: failed to reproject geometry to '%s' — hiding decal"), *GlobalCoordinates->CRS);
		OGRGeometryFactory::destroyGeometry(ProjectedGeometry);
		OGRCoordinateTransformation::DestroyCT(Transform);
		HideGeometryPreview();
		return;
	}
	OGRCoordinateTransformation::DestroyCT(Transform);

	FColor FillColor = PreviewColor.ToFColor(true);

	TArray<FColor> Colors;
	int Width, Height;
	if (!GDALInterface::RasterizeGeometry(ProjectedGeometry, PreviewResolution, Colors, Width, Height, FillColor))
	{
		UE_LOG(LogSplineImporter, Error, TEXT("UpdateGeometryPreview: RasterizeGeometry failed"));
		OGRGeometryFactory::destroyGeometry(ProjectedGeometry);
		HideGeometryPreview();
		return;
	}

	OGREnvelope Envelope;
	ProjectedGeometry->getEnvelope(&Envelope);

	FVector2D XY0 = FVector2D::ZeroVector;
	FVector2D XY1 = FVector2D::ZeroVector;
	bool bOk =
		GlobalCoordinates->GetUnrealCoordinatesFromCRS(Envelope.MinX, Envelope.MinY, GlobalCoordinates->CRS, XY0) &&
		GlobalCoordinates->GetUnrealCoordinatesFromCRS(Envelope.MaxX, Envelope.MaxY, GlobalCoordinates->CRS, XY1);

	OGRGeometryFactory::destroyGeometry(ProjectedGeometry);

	if (!bOk)
	{
		HideGeometryPreview();
		return;
	}

	FVector2D Min(FMath::Min(XY0.X, XY1.X), FMath::Min(XY0.Y, XY1.Y));
	FVector2D Max(FMath::Max(XY0.X, XY1.X), FMath::Max(XY0.Y, XY1.Y));

	FVector Center((Min.X + Max.X) / 2, (Min.Y + Max.Y) / 2, GetActorLocation().Z);
	double HalfExtentX = (Max.X - Min.X) / 2;
	double HalfExtentY = (Max.Y - Min.Y) / 2;

	TWeakObjectPtr<AOGRGeometry> WeakThis(this);
	Concurrency::RunOnGameThreadThrottled([WeakThis, Colors = MoveTemp(Colors), Width, Height, Center, HalfExtentX, HalfExtentY]() mutable
	{
		if (!WeakThis.IsValid())
		{
			UE_LOG(LogSplineImporter, Error, TEXT("UpdateGeometryPreview: actor invalid on game thread"));
			return;
		}
		if (!IsValid(WeakThis->PreviewDecal))
		{
			UE_LOG(LogSplineImporter, Error, TEXT("UpdateGeometryPreview: PreviewDecal component invalid on game thread"));
			return;
		}
		if (!IsValid(WeakThis->PreviewMaterial))
		{
			UE_LOG(LogSplineImporter, Error, TEXT("UpdateGeometryPreview: PreviewMaterial became invalid on game thread"));
			return;
		}

		UDecalComponent* Decal = WeakThis->PreviewDecal;
		if (!IsValid(Decal))
		{
			UE_LOG(LogSplineImporter, Error, TEXT("UpdateGeometryPreview: Invalid Decal"));
			return;
		}
		Decal->SetWorldLocation(Center);
		Decal->SetWorldRotation(FRotator(-90, 0, 0));
		Decal->DecalSize = FVector(1000000, HalfExtentY, HalfExtentX);

		UMaterialInstanceDynamic* MID = UDecalCoordinates::SetDecalTextureFromColors(
			Decal, WeakThis->PreviewMaterial, Width, Height, Colors, WeakThis.Get(), TEXT("T_OGRPreview")
		);

		if (!IsValid(MID))
		{
			UE_LOG(LogSplineImporter, Error, TEXT("UpdateGeometryPreview: SetDecalTextureFromColors returned null — decal material was NOT set"));
			return;
		}

		Decal->SetVisibility(true);
	});
}

void AOGRGeometry::HideGeometryPreview()
{
	TWeakObjectPtr<AOGRGeometry> WeakThis(this);
	Concurrency::RunOnGameThreadThrottled([WeakThis]()
	{
		if (!WeakThis.IsValid() || !IsValid(WeakThis->PreviewDecal)) return;
		WeakThis->PreviewDecal->SetVisibility(false);
	});
}

bool AOGRGeometry::Cleanup_Implementation(bool bSkipPrompt)
{
	TWeakObjectPtr<AOGRGeometry> WeakThis(this);

	if (!Concurrency::RunOnGameThreadAndWait([WeakThis]() {
		if (!WeakThis.IsValid()) return false;
		WeakThis->Modify();
		return true;
	}))
	{
		UE_LOG(LogSplineImporter, Warning, TEXT("Cleanup_Implementation: actor no longer valid before Modify() could run -- aborting"));
		return false;
	}

	if (DeleteGeneratedObjects(bSkipPrompt))
	{
		SetGeometry(nullptr);
		AlreadyHandledFeatures.Empty();
		return true;
	}
	else
	{
		return false;
	}
}

void AOGRGeometry::PostLoad()
{
	Super::PostLoad();
	
	RebuildGeometryFromSerialized();
}

void AOGRGeometry::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	RebuildGeometryFromSerialized();
	UpdateGeometryPreview();
}

void AOGRGeometry::BeginDestroy()
{
	{
		FScopeLock Lock(&GeometryLock);
		if (Geometry)
		{
			OGRGeometryFactory::destroyGeometry(Geometry);
			Geometry = nullptr;
		}
	}
	Super::BeginDestroy();
}

OGRGeometry* AOGRGeometry::CloneGeometry() const
{
	FScopeLock Lock(&GeometryLock);
	return Geometry ? Geometry->clone() : nullptr;
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

		NewOGRGeometry->RebuildGeometryFromSerialized();
		NewOGRGeometry->UpdateGeometryPreview();

		return NewOGRGeometry;
	}
	else
	{
		LCReporter::ShowError(LOCTEXT("AOGRGeometry::DuplicateActor", "Failed to duplicate actor."));
		return nullptr;
	}
}

void AOGRGeometry::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
	Super::PostEditChangeProperty(Event);

	if (!Event.Property) return;

	FName PropertyName = Event.Property->GetFName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AOGRGeometry, bShowGeometryPreview) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AOGRGeometry, PreviewResolution) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AOGRGeometry, PreviewColor) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AOGRGeometry, PreviewMaterial))
	{
		UpdateGeometryPreview();
	}
}

#endif

#undef LOCTEXT_NAMESPACE
