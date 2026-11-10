// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "SplineImporter/OGRGeometry.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "ConcurrencyHelpers/Concurrency.h"

#include "Components/DecalComponent.h"
#include "Coordinates/DecalCoordinates.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "Landscape.h"
#include "Engine/Texture2D.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#endif
#include "Coordinates/GlobalCoordinates.h"
#include "Coordinates/LevelCoordinates.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OGRGeometry)

#define LOCTEXT_NAMESPACE "FSplineImporterModule"

namespace
{
// Appends every polygon found in G (recursing into multis/collections/curves) to Dest. No GEOS needed.
int AppendPolygons(OGRMultiPolygon* Dest, const OGRGeometry* G)
{
if (!Dest || !G || G->IsEmpty()) return 0;
switch (wkbFlatten(G->getGeometryType()))
{
case wkbPolygon:
return Dest->addGeometry(G) == OGRERR_NONE ? 1 : 0;
case wkbCurvePolygon:
case wkbMultiSurface:
{
OGRGeometry* Linear = G->getLinearGeometry();
if (!Linear) return 0;
const int N = AppendPolygons(Dest, Linear);
OGRGeometryFactory::destroyGeometry(Linear);
return N;
}
case wkbMultiPolygon:
case wkbGeometryCollection:
{
const OGRGeometryCollection* C = G->toGeometryCollection();
int N = 0;
for (int i = 0, n = C->getNumGeometries(); i < n; ++i)
{
N += AppendPolygons(Dest, C->getGeometryRef(i));
}
return N;
}
default:
return 0;
}
}
}

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

	if (!Concurrency::RunOnGameThreadAndWait([WeakThis]() {
		if (!WeakThis.IsValid()) return false;
		FScopeLock Lock(&WeakThis->GeometryLock);
		if (WeakThis->bClearGeometryBeforeImporting || !WeakThis->Geometry)
		{
			if (WeakThis->Geometry) OGRGeometryFactory::destroyGeometry(WeakThis->Geometry);
			WeakThis->Geometry = OGRGeometryFactory::createGeometry(OGRwkbGeometryType::wkbMultiPolygon);
		}
		return true;
	}))
	{
		UE_LOG(LogSplineImporter, Warning, TEXT("OnGenerate: actor no longer valid before creating geometry -- aborting"));
		return false;
	}

	if (!Geometry)
	{
		LCReporter::ShowError(LOCTEXT("AOGRGeometry::OnGenerate::NoGeometry", "Internal error while creating geometry, please try again."));
		return false;
	}
	
{
// a geometry restored from older serialized data may not be a MultiPolygon
FScopeLock Lock(&GeometryLock);
if (wkbFlatten(Geometry->getGeometryType()) != wkbMultiPolygon)
{
OGRMultiPolygon* Normalized = OGRGeometryFactory::createGeometry(OGRwkbGeometryType::wkbMultiPolygon)->toMultiPolygon();
AppendPolygons(Normalized, Geometry);
OGRGeometryFactory::destroyGeometry(Geometry);
Geometry = Normalized;
}
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

			if (!GDALInterface::AddFeature(AlreadyHandledFeatures, Feature.get())) continue;

			OGRGeometry* NewGeometry = Feature->GetGeometryRef();
			if (!NewGeometry) continue;

int Added = 0;
{
FScopeLock Lock(&GeometryLock);
Added = AppendPolygons(Geometry->toMultiPolygon(), NewGeometry);
}
if (Added > 0) NumGeometries++;
		}
	}

	UE_LOG(LogSplineImporter, Log, TEXT("Found %d geometries"), NumGeometries);
	Tags.AddUnique(AreaTag);
	SetGeometry(Geometry);
	if (!MaskAssetPath.IsEmpty()) ExportMask(bIsUserInitiated);

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

bool AOGRGeometry::ExportMask(bool bIsUserInitiated)
{
#if WITH_EDITOR
	if (!FPackageName::IsValidLongPackageName(MaskAssetPath))
	{
		LCReporter::ShowError(LOCTEXT("AOGRGeometry::ExportMask::BadPath", "Mask Asset Path must look like /Game/Masks/T_Forest."));
		return false;
	}

	UGlobalCoordinates* GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(GetWorld());
	if (!IsValid(GlobalCoordinates)) return false;

	double WorldMinX = 0, WorldMaxX = 0, WorldMinY = 0, WorldMaxY = 0;
	int32 SizeX = 0, SizeY = 0;
	if (BoundingMethod != EBoundingMethod::BoundingActor)
	{
		LCReporter::ShowError(LOCTEXT("AOGRGeometry::ExportMask::NeedActor", "Mask export requires Bounding Method = Bounding Actor."));
		return false;
	}
	const FActorSelection Selection = BoundingActorSelection;
	const int32 Resolution = FMath::Clamp(MaskResolution, 2, 16384);

	if (!Concurrency::RunOnGameThreadAndWait([&]() {
		AActor* Target = Selection.GetActor(GetWorld());
		if (!IsValid(Target)) return false;

		if (ALandscape* Landscape = Cast<ALandscape>(Target))
		{
			FVector2D MinMaxX, MinMaxY, MinMaxZ;
			if (!LandscapeUtils::GetLandscapeBounds(Landscape, MinMaxX, MinMaxY, MinMaxZ)) return false;
			SizeX = Landscape->ComputeComponentCounts().X * Landscape->ComponentSizeQuads + 1;
			SizeY = Landscape->ComputeComponentCounts().Y * Landscape->ComponentSizeQuads + 1;
			if (SizeX < 2 || SizeY < 2) return false;
			const double StepX = (MinMaxX.Y - MinMaxX.X) / (SizeX - 1);
			const double StepY = (MinMaxY.Y - MinMaxY.X) / (SizeY - 1);
			WorldMinX = MinMaxX.X - StepX / 2;
			WorldMaxX = MinMaxX.Y + StepX / 2;
			WorldMinY = MinMaxY.X - StepY / 2;
			WorldMaxY = MinMaxY.Y + StepY / 2;
		}
		else
		{
			const FBox Box = Target->GetComponentsBoundingBox(true);
			if (!Box.IsValid) return false;
			WorldMinX = Box.Min.X; WorldMaxX = Box.Max.X;
			WorldMinY = Box.Min.Y; WorldMaxY = Box.Max.Y;
			const double DX = WorldMaxX - WorldMinX;
			const double DY = WorldMaxY - WorldMinY;
			if (DX <= 0 || DY <= 0) return false;
			SizeX = DX >= DY ? Resolution : FMath::Max(2, FMath::RoundToInt(Resolution * DX / DY));
			SizeY = DX >= DY ? FMath::Max(2, FMath::RoundToInt(Resolution * DY / DX)) : Resolution;
		}
		return true;
	}))
	{
		LCReporter::ShowError(LOCTEXT("AOGRGeometry::ExportMask::NoBounds", "Could not get the extent of the Bounding Actor."));
		return false;
	}

	double MinCX = DBL_MAX, MaxCX = -DBL_MAX, MinCY = DBL_MAX, MaxCY = -DBL_MAX;
	for (const double X : { WorldMinX, WorldMaxX })
	{
		for (const double Y : { WorldMinY, WorldMaxY })
		{
			FVector2D C;
			GlobalCoordinates->GetCRSCoordinatesFromUnrealLocation(FVector2D(X, Y), C);
			MinCX = FMath::Min(MinCX, C.X); MaxCX = FMath::Max(MaxCX, C.X);
			MinCY = FMath::Min(MinCY, C.Y); MaxCY = FMath::Max(MaxCY, C.Y);
		}
	}

	OGRGeometry* Projected = CloneGeometry();
	if (!Projected)
	{
		LCReporter::ShowError(LOCTEXT("AOGRGeometry::ExportMask::NoGeometry", "There is no geometry to export, please generate first."));
		return false;
	}
	ON_SCOPE_EXIT { OGRGeometryFactory::destroyGeometry(Projected); };

	OGRCoordinateTransformation* Transform = GlobalCoordinates->GetCRSTransformer("EPSG:4326");
	if (!Transform) return false;
	const OGRErr TransformErr = Projected->transform(Transform);
	OGRCoordinateTransformation::DestroyCT(Transform);
	if (TransformErr != OGRERR_NONE)
	{
		LCReporter::ShowError(LOCTEXT("AOGRGeometry::ExportMask::Reproject", "Could not reproject the geometry to the coordinate system of the GlobalCoordinates."));
		return false;
	}

	TArray<uint8> Mask;
	if (!GDALInterface::RasterizeGeometryMask(Projected, MinCX, MaxCX, MinCY, MaxCY, SizeX, SizeY, Mask, false))
	{
		LCReporter::ShowError(LOCTEXT("AOGRGeometry::ExportMask::Rasterize", "Could not rasterize the geometry."));
		return false;
	}

	const FString Path = MaskAssetPath;
	const bool bSaved = Concurrency::RunOnGameThreadAndWait([&]() {
		const FString AssetName = FPackageName::GetLongPackageAssetName(Path);
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("%s.%s"), *Path, *AssetName), nullptr, LOAD_NoWarn | LOAD_Quiet);
		const bool bNew = !IsValid(Texture);
		if (bNew)
		{
			UPackage* Package = CreatePackage(*Path);
			if (!Package) return false;
			Texture = NewObject<UTexture2D>(Package, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
		}
		if (!IsValid(Texture)) return false;

		Texture->Modify();
		Texture->Source.Init(SizeX, SizeY, 1, 1, TSF_G8, Mask.GetData());
		Texture->SRGB = false;
		Texture->CompressionSettings = TC_Grayscale;
		Texture->MipGenSettings = TMGS_FromTextureGroup;
		Texture->LODGroup = TEXTUREGROUP_World;
		Texture->UpdateResource();
		Texture->PostEditChange();
		Texture->MarkPackageDirty();
		if (bNew) FAssetRegistryModule::AssetCreated(Texture);
		return true;
	});

	if (!bSaved)
	{
		LCReporter::ShowError(LOCTEXT("AOGRGeometry::ExportMask::Asset", "Could not create the mask texture asset."));
		return false;
	}

	UE_LOG(LogSplineImporter, Log, TEXT("Exported mask %s (%dx%d). World extent: X [%f, %f], Y [%f, %f]. Row 0 is the north row of the CRS."),
		*Path, SizeX, SizeY, WorldMinX, WorldMaxX, WorldMinY, WorldMaxY);

	if (bIsUserInitiated)
	{
		Concurrency::RunOnGameThreadAndWait([&]() {
			LCReporter::ShowInfo(
				FText::Format(
					LOCTEXT("AOGRGeometry::ExportMask::Done", "Exported mask {0} ({1}x{2})."),
					FText::FromString(Path), FText::AsNumber(SizeX), FText::AsNumber(SizeY)
				),
				"SuppressMaskExportInfo",
				LOCTEXT("AOGRGeometry::ExportMask::DoneTitle", "Mask exported")
			);
			return true;
		});
	}
	return true;
#else
	UE_LOG(LogSplineImporter, Warning, TEXT("ExportMask is only supported in the editor"));
	return false;
#endif
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
	if (GetGeneratorStatus() != EGeneratorStatus::Generating) Concurrency::SetCancelRequested(false);
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
