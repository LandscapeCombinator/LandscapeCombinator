// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "Coordinates/LevelCoordinates.h"
#if WITH_EDITOR
#include "UDynamicMesh.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "AssetUtils/CreateStaticMeshUtil.h"
#include "PhysicsEngine/BodySetup.h"
#endif
#include "Coordinates/BodyNavigation.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Misc/EngineVersionComparison.h"
#include "EngineUtils.h"
#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "SceneViewExtension.h"
#endif
#include "Coordinates/DecalCoordinates.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GDALInterface/GDALInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "FileDownloader/Download.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Engine/DecalActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h" 
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h" 
#include "Stats/Stats.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"
#include "Materials/Material.h"

#define LOCTEXT_NAMESPACE "FCoordinatesModule"

ALevelCoordinates::ALevelCoordinates()
{
	PrimaryActorTick.bCanEverTick = false;

	GlobalCoordinates = CreateDefaultSubobject<UGlobalCoordinates>(TEXT("Global Coordinates"));
}

TObjectPtr<UGlobalCoordinates> ALevelCoordinates::GetGlobalCoordinates(TWeakObjectPtr<UWorld> World, bool bShowDialog)
{
	TArray<TWeakObjectPtr<AActor>> LevelCoordinatesCandidates0;

	if (!Concurrency::RunOnGameThreadAndWait([World, &LevelCoordinatesCandidates0]()
	{
		if (!World.IsValid()) return false;

		TArray<AActor*> All;
		UGameplayStatics::GetAllActorsOfClass(World.Get(), ALevelCoordinates::StaticClass(), All);

		for (AActor* Actor : All)
		{
			if (IsValid(Actor) && !Actor->IsHidden())
				LevelCoordinatesCandidates0.Add(Actor);
		}
		return true;
	}))
	{
		return nullptr;
	}

	TArray<TWeakObjectPtr<AActor>> LevelCoordinatesCandidates = LevelCoordinatesCandidates0
		.FilterByPredicate([](const TWeakObjectPtr<AActor>& Actor) { return Actor.IsValid(); });

	if (LevelCoordinatesCandidates.Num() == 0)
	{
		if (bShowDialog) LCReporter::ShowError(LOCTEXT("NoLevelCoordinates", "Could not find a visible Level Coordinates actor in the level. Please add one, or unhide the existing one."));
		return nullptr;
	}
	if (LevelCoordinatesCandidates.Num() > 1)
	{
		if (bShowDialog) LCReporter::ShowError(LOCTEXT("MoreThanOneLevelCoordinates", "Found more than one visible Level Coordinates actor in the level. Please keep only one."));
		return nullptr;
	}

	return Cast<ALevelCoordinates>(LevelCoordinatesCandidates[0].Get())->GlobalCoordinates;
}


OGRCoordinateTransformation *ALevelCoordinates::GetCRSTransformer(UWorld* World, FString CRS)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return nullptr;
	return GlobalCoordinates->GetCRSTransformer(CRS);
}

bool ALevelCoordinates::GetUnrealCoordinatesFromCRS(UWorld* World, double Longitude, double Latitude, FString CRS, FVector2D& OutXY)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;
	if (!GlobalCoordinates->GetUnrealCoordinatesFromCRS(Longitude, Latitude, CRS, OutXY)) return false;

	return true;
}

bool ALevelCoordinates::GetCRSCoordinatesFromUnrealLocation(UWorld* World, FVector2D Location, FVector2D& OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;
	GlobalCoordinates->GetCRSCoordinatesFromUnrealLocation(Location, OutCoordinates);
	return true;
}

bool ALevelCoordinates::GetCRSCoordinatesFromUnrealLocation(UWorld* World, FVector2D Location, FString CRS, FVector2D &OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;
	if (!GlobalCoordinates->GetCRSCoordinatesFromUnrealLocation(Location, CRS, OutCoordinates)) return false;

	return true;
}

bool ALevelCoordinates::GetCRSCoordinatesFromUnrealLocations(UWorld* World, FVector4d Locations, FString CRS, FVector4d &OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;
	if (!GlobalCoordinates->GetCRSCoordinatesFromUnrealLocations(Locations, CRS, OutCoordinates)) return false;

	return true;
}

bool ALevelCoordinates::GetCRSCoordinatesFromUnrealLocations(UWorld* World, FVector4d Locations, FVector4d& OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;

	GlobalCoordinates->GetCRSCoordinatesFromUnrealLocations(Locations, OutCoordinates);
	return true;
}

bool ALevelCoordinates::GetCRSCoordinatesFromFBox(UWorld* World, FBox Box, FString ToCRS, FVector4d& OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;

	return GlobalCoordinates->GetCRSCoordinatesFromFBox(Box, ToCRS, OutCoordinates);
}

bool ALevelCoordinates::GetCRSCoordinatesFromOriginExtent(UWorld* World, FVector Origin, FVector Extent, FString ToCRS, FVector4d& OutCoordinates)
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(World);
	if (!GlobalCoordinates) return false;
	
	return GlobalCoordinates->GetCRSCoordinatesFromOriginExtent(Origin, Extent, ToCRS, OutCoordinates);
}

void ALevelCoordinates::MatchSkyAtmosphereToBody()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(GlobalCoordinates)) return;

	int32 Count = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		USkyAtmosphereComponent* Sky = It->FindComponentByClass<USkyAtmosphereComponent>();
		if (!IsValid(Sky)) continue;

		Sky->Modify();
		Sky->TransformMode = ESkyAtmosphereTransformMode::PlanetTopAtAbsoluteWorldOrigin;
		Sky->BottomRadius = GlobalCoordinates->BodyRadiusCm / 100000.0;
		Sky->MarkRenderStateDirty();
		Count++;
	}

	if (Count == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("MatchSkyAtmosphereToBody: no SkyAtmosphere actor found in the level"));
	}
}

#if WITH_EDITOR
namespace
{
	UStaticMesh* CreateSphereStaticMesh(UObject* Outer, int32 Steps, double Radius)
	{
		const FString AssetName = FString::Printf(TEXT("SM_Sphere_%d_%lld"), Steps, FMath::RoundToInt64(Radius));
		const FString AssetPath = FString("/Game/LandscapeCombinator/Generated/Spheres/") + AssetName;
		if (UStaticMesh* Existing = LoadObject<UStaticMesh>(nullptr, *(AssetPath + "." + AssetName), nullptr, LOAD_NoWarn)) return Existing;

		UDynamicMesh* SphereMesh = NewObject<UDynamicMesh>(Outer);
		UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSphereBox(
			SphereMesh, FGeometryScriptPrimitiveOptions(), FTransform(),
			Radius, Steps, Steps, Steps, EGeometryScriptPrimitiveOriginMode::Center
		);

		UE::AssetUtils::FStaticMeshAssetOptions Options;
		Options.NewAssetPath = AssetPath;
		Options.NumMaterialSlots = 1;
		Options.SourceMeshes.DynamicMeshes.Add(SphereMesh->GetMeshPtr());

		UE::AssetUtils::FStaticMeshResults Results;
		if (UE::AssetUtils::CreateStaticMeshAsset(Options, Results) != UE::AssetUtils::ECreateStaticMeshResult::Ok || !IsValid(Results.StaticMesh))
		{
			LCReporter::ShowError(LOCTEXT("ALevelCoordinates::SpawnSphere::NoMesh", "Could not create the sphere static mesh, please check log output."));
			return nullptr;
		}

		UStaticMesh* Mesh = Results.StaticMesh;
		Mesh->Modify();
		if (!Mesh->GetBodySetup()) Mesh->CreateBodySetup();
		if (UBodySetup* BodySetup = Mesh->GetBodySetup())
		{
			BodySetup->Modify();
			BodySetup->RemoveSimpleCollision();
			BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseComplexAsSimple;
			BodySetup->InvalidatePhysicsData();
			BodySetup->CreatePhysicsMeshes();
		}
		Mesh->MarkPackageDirty();
		return Mesh;
	}

	AStaticMeshActor* FindOrSpawnSphereActor(UWorld* World, const FVector& Center)
	{
		static const FName SphereTag("LCBodySphere");
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(SphereTag))
			{
				It->Modify();
				return *It;
			}
		}

		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Center, FRotator::ZeroRotator);
		if (!IsValid(Actor)) return nullptr;
		Actor->Tags.Add(SphereTag);
		Actor->SetActorLabel(TEXT("BodySphere"));
		return Actor;
	}
}

bool ALevelCoordinates::SpawnSphere()
{
	UWorld* World = GetWorld();
	if (!IsValid(World)) return false;

	UStaticMesh* Mesh = CreateSphereStaticMesh(this, SphereSteps, GlobalCoordinates->BodyRadiusCm);
	if (!IsValid(Mesh)) return false;

	const FVector Center(0, 0, -GlobalCoordinates->BodyRadiusCm);

	AStaticMeshActor* SphereActor = FindOrSpawnSphereActor(World, Center);
	if (!IsValid(SphereActor)) return false;

	UStaticMeshComponent* Component = SphereActor->GetStaticMeshComponent();
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(Mesh);
	Component->SetCollisionProfileName(TEXT("BlockAll"));
	Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Component->SetCastShadow(false);
	SphereActor->SetActorLocation(Center);
	SphereActor->SetActorScale3D(FVector::OneVector);
	Component->SetMobility(EComponentMobility::Static);
	return true;
}
#endif

bool ALevelCoordinates::IsSphereModel() const
{
	return UGlobalCoordinates::IsSphere(GlobalCoordinates);
}

void ALevelCoordinates::UpdateBodyParameters()
{
	UWorld* World = GetWorld();
	if (IsTemplate() || !IsValid(World) || !IsValid(GlobalCoordinates)) return;

	UMaterialParameterCollection* Collection = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/LandscapeCombinator/Materials/MPC_LCBody.MPC_LCBody"), nullptr, LOAD_NoWarn);
	if (!IsValid(Collection)) return;

	UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(Collection);
	if (!IsValid(Instance)) return;

	const FLinearColor Body(
		(float) UGlobalCoordinates::GetSphereRadiusCm(GlobalCoordinates),
		(float) GlobalCoordinates->WorldOriginLat,
		(float) GlobalCoordinates->WorldOriginLong,
		GlobalCoordinates->IsSphere() ? 1.f : 0.f
	);
	Instance->SetVectorParameterValue(FName("Body"), Body);
}

void ALevelCoordinates::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	UpdateViewportTicker();
	UpdateBodyParameters();
}

void ALevelCoordinates::BeginDestroy()
{
	if (ViewportTickHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(ViewportTickHandle);
	Super::BeginDestroy();
}

#if WITH_EDITOR
void ALevelCoordinates::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateViewportTicker();
}
#endif

void ALevelCoordinates::SetSphereViewportNavigation(bool bEnabled)
{
	Modify();
	bSphereViewportNavigation = bEnabled;
	UpdateViewportTicker();
}

#if WITH_EDITOR && !UE_VERSION_OLDER_THAN(5, 2, 0)
#define LC_VIEWPORT_EXTENSION 1
#else
#define LC_VIEWPORT_EXTENSION 0
#endif

#if LC_VIEWPORT_EXTENSION
class FBodyViewportExtension : public FSceneViewExtensionBase
{
public:
	FBodyViewportExtension(const FAutoRegister& AutoRegister, TFunction<void()> InCallback)
		: FSceneViewExtensionBase(AutoRegister), Callback(MoveTemp(InCallback)) {}

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override { if (Callback) Callback(); }
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override {}
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override {}

private:
	TFunction<void()> Callback;
};
#endif

void ALevelCoordinates::UpdateViewportTicker()
{
#if WITH_EDITOR
	if (ViewportTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ViewportTickHandle);
		ViewportTickHandle.Reset();
	}
	ViewportExtension.Reset();
	ViewportLastLocations.Reset();
	ViewportLastRotators.Reset();
	UWorld* ThisWorld = GetWorld();
	if (!bSphereViewportNavigation || IsTemplate() || !ThisWorld || ThisWorld->WorldType != EWorldType::Editor) return;
	ViewportTickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ALevelCoordinates::TickViewport), 0.0f);
#if LC_VIEWPORT_EXTENSION
	TWeakObjectPtr<ALevelCoordinates> WeakThis(this);
	ViewportExtension = FSceneViewExtensions::NewExtension<FBodyViewportExtension>([WeakThis]()
	{
		if (ALevelCoordinates* Self = WeakThis.Get()) Self->TickViewport(0.0f);
	});
#endif
#endif
}

bool ALevelCoordinates::TickViewport(float DeltaTime)
{
#if WITH_EDITOR
	if (!GEditor || !IsValid(GlobalCoordinates)) return true;
	if (!GlobalCoordinates->IsSphere())
	{
		ViewportLastLocations.Reset();
		ViewportLastRotators.Reset();
		return true;
	}
	for (FEditorViewportClient* VC : GEditor->GetLevelViewportClients())
	{
		if (!VC || !VC->IsPerspective()) continue;
		void* Key = static_cast<void*>(VC);
		const FVector CurrentLocation = VC->GetViewLocation();
		const FRotator CurrentRotation = VC->GetViewRotation();
		FVector Location = CurrentLocation;
		FRotator Rotation = CurrentRotation;
		FBodyNavigation::Apply(*GlobalCoordinates, ViewportLastLocations.Find(Key), ViewportLastRotators.Find(Key), Location, Rotation);

		if (!Location.Equals(CurrentLocation, 0.01) || !Rotation.Equals(CurrentRotation, 0.001))
		{
			VC->SetViewLocation(Location);
			VC->SetViewRotation(Rotation);
			VC->Invalidate();
		}
		ViewportLastLocations.Add(Key, VC->GetViewLocation());
		ViewportLastRotators.Add(Key, VC->GetViewRotation());
	}
#endif
	return true;
}

void ALevelCoordinates::CreateWorldMap()
{
	if (!GlobalCoordinates)
	{
		LCReporter::ShowError(
			LOCTEXT("ALevelCoordinates::CreateWorldMap::1", "This LevelCoordinates Actor doesn't have GlobalCoordinates.")
		);
		return;
	}

	if (GlobalCoordinates->IsSphere())
	{
#if WITH_EDITOR
		if (!SpawnSphere()) return;
#endif
		MatchSkyAtmosphereToBody();
	}

	if (!IsValid(GlobalCoordinates->IsSphere() ? WorldMapSphereMaterial : WorldMapMaterial)) return;

	if (bUseLocalFile)
	{
		CreateWorldMapFromFile(PathToGeoreferencedWorldMap);
	}
	else
	{
		double MinLong = -179.999989;
		double MaxLong = 179.999988;
		double MinLat = -89.000000;
		double MaxLat = 89.000000;

		FString URL = FString::Format(
			TEXT("https://basemap.nationalmap.gov:443/arcgis/services/USGSImageryOnly/MapServer/WmsServer?LAYERS=0&FORMAT=image/tiff&SERVICE=WMS&VERSION=1.3.0&REQUEST=GetMap&CRS=CRS:84&STYLES=&BBOX={0},{1},{2},{3}&WIDTH={4}&HEIGHT={5}"),
			{ 
				MinLong, MinLat, MaxLong, MaxLat,
				Width, Height
			}
		);
	
		FString Intermediate = FPaths::ConvertRelativePathToFull(FPaths::EngineIntermediateDir());
		FString CoordinatesDir = FPaths::Combine(Intermediate, "Coordinates");
		FString DownloadedWorldMapPath = FPaths::Combine(CoordinatesDir, "DownloadedWorldMap.tif");
		FString TempWorldMapPath = FPaths::Combine(CoordinatesDir, "TempWorldMap.tif");
		FString WorldMapPath = FPaths::Combine(CoordinatesDir, "WorldMap.tif");
		Download::FromURL(URL, DownloadedWorldMapPath, true, [this, MinLong, MaxLong, MinLat, MaxLat, DownloadedWorldMapPath, TempWorldMapPath, WorldMapPath](bool bSuccess) {
			if (bSuccess)
			{
				bool bSuccessWriteCRS = GDALInterface::AddGeoreference(DownloadedWorldMapPath, WorldMapPath, "EPSG:4326", MinLong, MaxLong, MinLat, MaxLat);
				if (bSuccessWriteCRS)
				{
					CreateWorldMapFromFile(WorldMapPath);
				}
				else
				{
					LCReporter::ShowError(
						LOCTEXT("ALevelCoordinates::CreateWorldMap", "Could not write coordinate system to world map.")
					);
				}
			}
			else
			{
				LCReporter::ShowError(
					LOCTEXT("ALevelCoordinates::CreateWorldMap", "Could not download world map from USGS Imagery.")
				);
			}
		});
	}
}

void ALevelCoordinates::CreateWorldMapFromFile(FString Path)
{
	UMaterialInterface* Material = GlobalCoordinates->IsSphere() ? WorldMapSphereMaterial : WorldMapMaterial;
	if (!IsValid(Material)) return;

	if (GlobalCoordinates->IsSphere())
	{
		int ImageWidth, ImageHeight;
		TArray<FColor> Colors;
		if (!GDALInterface::ReadColorsFromFile(Path, ImageWidth, ImageHeight, Colors))
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("ALevelCoordinates::CreateWorldMapFromFile::ReadColors", "Could not read colors from file {0}."),
				FText::FromString(Path)
			));
			return;
		}

		UTexture2D* NewTexture = UDecalCoordinates::CreateDecalTexture(ImageWidth, ImageHeight, Colors, this, TEXT("T_WorldMapSphere"), true);
		if (!IsValid(NewTexture)) return;

		static const FName SphereTag("LCBodySphere");
		UStaticMeshComponent* SphereComponent = nullptr;
		for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
		{
			if (It->ActorHasTag(SphereTag)) { SphereComponent = It->GetStaticMeshComponent(); break; }
		}
		if (!IsValid(SphereComponent))
		{
			LCReporter::ShowError(LOCTEXT("ALevelCoordinates::CreateWorldMapFromFile::NoSphere", "Could not find the body sphere actor."));
			return;
		}

		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Material, this);
		MID->SetTextureParameterValue(FName("Texture"), NewTexture);
		GlobalCoordinates->SetSphereMappingParameters(MID, FVector2D(-180.0, -90.0), FVector2D(180.0, 90.0));
		SphereComponent->SetMaterial(0, MID);
		return;
	}

	FVector4d Coordinates;
	ADecalActor* DecalActor = UDecalCoordinates::CreateDecal(GetWorld(), Material, Path, Coordinates);
	if (!IsValid(DecalActor)) return;
	
	AStaticMeshActor* PlaneActor = GetWorld()->SpawnActor<AStaticMeshActor>();
#if WITH_EDITOR
	PlaneActor->SetActorLabel("WorldMapPlane");
#endif
	PlaneActor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane")));
	PlaneActor->SetActorLocation(FVector((Coordinates[1] + Coordinates[0]) / 2, (Coordinates[2] + Coordinates[3]) / 2, 0));

	// Z-scale = 1 has weird lighting issues when X and Y are large, so we use Z-scale = X-scale
	PlaneActor->SetActorScale3D(FVector((Coordinates[1] - Coordinates[0]) / 100, (Coordinates[2] - Coordinates[3]) / 100, (Coordinates[1] - Coordinates[0]) / 100));
}

#undef LOCTEXT_NAMESPACE
