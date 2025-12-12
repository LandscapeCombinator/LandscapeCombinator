// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeUtils/LandscapeUtils.h"
#include "LandscapeUtils/LogLandscapeUtils.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "Coordinates/LevelCoordinates.h"
#include "GDALInterface/GDALInterface.h"

#include "Kismet/KismetMathLibrary.h"
#include "Internationalization/Regex.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/EngineVersionComparison.h"
#include "Engine/World.h"

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
#include "LandscapeEditLayer.h"
#endif

#define LOCTEXT_NAMESPACE "FLandscapeUtilsModule"

void LandscapeUtils::MakeDataRelativeTo(int SizeX, int SizeY, uint16* Data, uint16* Base)
{
	for (int X = 0; X < SizeX; X++)
	{
		for (int Y = 0; Y < SizeY; Y++)
		{
			Data[X + Y * SizeX] = FMath::Min(FMath::Max(0, (((int32) Data[X + Y * SizeX]) + 32768) - Base[X + Y * SizeX]), (int32) MAX_uint16);
		}
	}
}

bool LandscapeUtils::GetLandscapeBounds(ALandscape *Landscape, TArray<ALandscapeStreamingProxy*> LandscapeStreamingProxies, FVector2D &MinMaxX, FVector2D &MinMaxY, FVector2D &MinMaxZ)
{
	FString LandscapeLabel = Landscape->GetActorNameOrLabel();

	if (LandscapeStreamingProxies.IsEmpty())
	{
		UE_LOG(LogLandscapeUtils, Log, TEXT("GetLandscapeBounds: Landscape %s has no streaming proxies."), *LandscapeLabel);
		FVector LandscapeOrigin;
		FVector LandscapeExtent;
		Landscape->GetActorBounds(false, LandscapeOrigin, LandscapeExtent, true);

		MinMaxX[0] = LandscapeOrigin.X - LandscapeExtent.X;
		MinMaxX[1] = LandscapeOrigin.X + LandscapeExtent.X;

		MinMaxY[0] = LandscapeOrigin.Y - LandscapeExtent.Y;
		MinMaxY[1] = LandscapeOrigin.Y + LandscapeExtent.Y;

		MinMaxZ[0] = LandscapeOrigin.Z - LandscapeExtent.Z;
		MinMaxZ[1] = LandscapeOrigin.Z + LandscapeExtent.Z;
		
		return true;
	}
	else
	{
		UE_LOG(LogLandscapeUtils, Log, TEXT("GetLandscapeBounds: Landscape %s has streaming proxies."), *LandscapeLabel);
		double MaxX = -DBL_MAX;
		double MinX = DBL_MAX;
		double MaxY = -DBL_MAX;
		double MinY = DBL_MAX;
		double MaxZ = -DBL_MAX;
		double MinZ = DBL_MAX;

		for (auto& LandscapeStreamingProxy : LandscapeStreamingProxies)
		{
			FVector LandscapeProxyOrigin;
			FVector LandscapeProxyExtent;
			LandscapeStreamingProxy->GetActorBounds(false, LandscapeProxyOrigin, LandscapeProxyExtent);
			
			MaxX = FMath::Max(MaxX, LandscapeProxyOrigin.X + LandscapeProxyExtent.X);
			MinX = FMath::Min(MinX, LandscapeProxyOrigin.X - LandscapeProxyExtent.X);
			MaxY = FMath::Max(MaxY, LandscapeProxyOrigin.Y + LandscapeProxyExtent.Y);
			MinY = FMath::Min(MinY, LandscapeProxyOrigin.Y - LandscapeProxyExtent.Y);
			MaxZ = FMath::Max(MaxZ, LandscapeProxyOrigin.Z + LandscapeProxyExtent.Z);
			MinZ = FMath::Min(MinZ, LandscapeProxyOrigin.Z - LandscapeProxyExtent.Z);
		}

		if (MaxX != -DBL_MAX && MinX != DBL_MAX && MaxY != -DBL_MAX && MinY != DBL_MAX && MaxZ != -DBL_MAX && MinZ != DBL_MAX)
		{
			MinMaxX[0] = MinX;
			MinMaxX[1] = MaxX;
			MinMaxY[0] = MinY;
			MinMaxY[1] = MaxY;
			MinMaxZ[0] = MinZ;
			MinMaxZ[1] = MaxZ;
			return true;
		}
		else
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("GetLandscapeBounds", "Could not compute Min and Max values for Landscape {0}."),
				FText::FromString(LandscapeLabel)
			));
			return false;
		}
	}
}

bool LandscapeUtils::GetLandscapeBounds(ALandscape* Landscape, FVector2D& MinMaxX, FVector2D& MinMaxY, FVector2D& MinMaxZ)
{
	return GetLandscapeBounds(Landscape, GetLandscapeStreamingProxies(Landscape), MinMaxX, MinMaxY, MinMaxZ);
}

bool LandscapeUtils::GetLandscapeMinMaxZ(ALandscape* Landscape, FVector2D& MinMaxZ)
{
	FVector2D UnusedMinMaxX, UnusedMinMaxY;
	return GetLandscapeBounds(Landscape, UnusedMinMaxX, UnusedMinMaxY, MinMaxZ);
}

TArray<ALandscapeStreamingProxy*> LandscapeUtils::GetLandscapeStreamingProxies(ALandscape* Landscape)
{
	TArray<AActor*> LandscapeStreamingProxiesTemp;
	Concurrency::RunOnGameThreadAndWait([&]() {
		UGameplayStatics::GetAllActorsOfClass(Landscape->GetWorld(), ALandscapeStreamingProxy::StaticClass(), LandscapeStreamingProxiesTemp);
		return true;
	});

	TArray<ALandscapeStreamingProxy*> LandscapeStreamingProxies;

	for (auto& LandscapeStreamingProxy0 : LandscapeStreamingProxiesTemp)
	{
		ALandscapeStreamingProxy* LandscapeStreamingProxy = Cast<ALandscapeStreamingProxy>(LandscapeStreamingProxy0);
		if (LandscapeStreamingProxy->GetLandscapeActor() == Landscape)
			LandscapeStreamingProxies.Add(LandscapeStreamingProxy);
	}

	return LandscapeStreamingProxies;
}

// Parameters to collide with this actor only, ignoring all other actors
bool LandscapeUtils::CustomCollisionQueryParams(AActor *Actor, FCollisionQueryParams &CollisionQueryParams)
{
	if (!IsValid(Actor))
	{
		LCReporter::ShowError(LOCTEXT("InvalidCollindingActor", "Invalid colliding actor"));
		return false;
	}

	UWorld *World = Actor->GetWorld();
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
	CollisionQueryParams = FCollisionQueryParams();

	if (Actor->IsA<ALandscape>())
	{
		ALandscape *Landscape = Cast<ALandscape>(Actor);
		TArray<ALandscapeStreamingProxy*> LandscapeStreamingProxies = GetLandscapeStreamingProxies(Landscape);
		for (auto &SomeActor : Actors)
		{
			if (SomeActor != Landscape && !LandscapeStreamingProxies.Contains(SomeActor))
			{
				CollisionQueryParams.AddIgnoredActor(SomeActor);
			}
		}
	}
	else
	{
		for (auto &SomeActor : Actors)
		{
			if (SomeActor != Actor)
			{
				CollisionQueryParams.AddIgnoredActor(SomeActor);
			}
		}
	}

	return true;
}

// Parameters to collide with these actors only, ignoring all other actors
bool LandscapeUtils::CustomCollisionQueryParams(TArray<AActor*> CollidingActors, FCollisionQueryParams &CollisionQueryParams)
{
	if (CollidingActors.IsEmpty() || !IsValid(CollidingActors[0]))
	{
		LCReporter::ShowError(LOCTEXT("InvalidCollidingActors", "Invalid colliding actors for custom collision query"));
		return false;
	}

	TSet<AActor*> CollidingActorsSet = TSet<AActor*>(CollidingActors);
	for (auto &CollidingActor: CollidingActors)
	{
		if (!IsValid(CollidingActor)) continue;

		if (ALandscape *Landscape = Cast<ALandscape>(CollidingActor))
		{
			TArray<ALandscapeStreamingProxy*> LandscapeStreamingProxies = GetLandscapeStreamingProxies(Landscape);
			for (auto &LandscapeStreamingProxy: LandscapeStreamingProxies) CollidingActorsSet.Add(LandscapeStreamingProxy);
		}
	}

	UWorld *World = nullptr;
	for (auto &CollidingActor: CollidingActors)
	{
		World = CollidingActor->GetWorld();
		if (IsValid(World)) break;
	}

	if (!IsValid(World)) return false;
	
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
	CollisionQueryParams = FCollisionQueryParams();

	for (auto &SomeActor : Actors)
	{
		if (!CollidingActorsSet.Contains(SomeActor))
		{
			CollisionQueryParams.AddIgnoredActor(SomeActor);
		}
	}

	return true;
}

bool LandscapeUtils::CustomCollisionQueryParams(const UWorld *World, const FActorSelection &ActorSelection, FCollisionQueryParams &CollisionQueryParams)
{
	return CustomCollisionQueryParams(ActorSelection.GetAllActors(World), CollisionQueryParams);
}

bool LandscapeUtils::GetZ(UWorld* World, FCollisionQueryParams CollisionQueryParams, double x, double y, double &OutZ, bool bDrawDebugLine)
{
	FVector StartLocation = FVector(x, y, HALF_WORLD_MAX);
	FVector EndLocation = FVector(x, y, -HALF_WORLD_MAX);

	FHitResult HitResult;
	bool bLineTrace = World->LineTraceSingleByChannel(
		OUT HitResult,
		StartLocation,
		EndLocation,
		ECollisionChannel::ECC_Visibility,
		CollisionQueryParams
	);
	bool bValidActor = IsValid(HitResult.GetActor());
	if (bLineTrace && !bValidActor)
	{
		UE_LOG(LogLandscapeUtils, Error, TEXT("Strange! Got a line trace hit on an invalid actor"));
		return false;
	}
	bool bHit = bLineTrace && bValidActor;

	if (bDrawDebugLine)
	{
		DrawDebugLine(
			World,
			StartLocation,
			EndLocation,
			bHit ? FColor::Green : FColor::Red,
			false,
			10,
			0,
			50
		);
	}

	if (bHit)
	{
		OutZ = HitResult.ImpactPoint.Z;
		return true;
	}
	else
	{
		return false;
	}
}

bool LandscapeUtils::GetZ(AActor *Actor, double x, double y, double &OutZ, bool bDrawDebugLine)
{
	if (!IsValid(Actor))
	{
		UE_LOG(LogLandscapeUtils, Error, TEXT("LandscapeUtils::GetZ: Actor is invalid"));
		return false;
	}

	UWorld *World = Actor->GetWorld();
	FCollisionQueryParams CollisionQueryParams;
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
	Actors.Remove(Actor);
	CollisionQueryParams.AddIgnoredActors(Actors);
	return GetZ(World, CollisionQueryParams, x, y, OutZ, bDrawDebugLine);
}

bool LandscapeUtils::CreateMeshFromHeightmap(
		int Width, int Height, const TArray<float>& Heightmap, const FVector2D & TopLeftCorner, const FVector2D & BottomRightCorner, double ZScale,
		TArray<FVector> &OutVertices, TArray<FIndex3i> &OutTriangles
	)
{
	// we use Delta to find the position at the center of the first/last pixels
	const FVector2D Delta = (BottomRightCorner - TopLeftCorner) / FVector2D(2 * Width, 2 * Height);
	const FVector2D FirstPixelPosition = TopLeftCorner + Delta;
	const FVector2D LastPixelPosition = BottomRightCorner - Delta;

	const float StepX = (LastPixelPosition.X - FirstPixelPosition.X) / Width;
	const float StepY = (LastPixelPosition.Y - FirstPixelPosition.Y) / Height;

	TArray<int> IndexMap;
	IndexMap.SetNum(Width * Height);

	OutVertices.Empty();
	OutTriangles.Empty();

	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("CreateMeshFromHeightmap::AppendVertexLoop");

		for (int X = 0; X < Width; X++)
		{
			for (int Y = 0; Y < Height; Y++)
			{
				const int k = X + Y * Width;
				const float v = Heightmap[k];

				IndexMap[X + Y * Width] = OutVertices.Num();

				OutVertices.Add(FVector3d(FirstPixelPosition.X + StepX * X, FirstPixelPosition.Y + StepY * Y, v * 100 * ZScale));
			}
		}
	}


	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("CreateMeshFromHeightmap::AppendTriangleLoop");

		for (int X = 0; X < Width - 1; X++)
		{
			for (int Y = 0; Y < Height - 1; Y++)
			{
				int V1 = IndexMap[X + Y * Width]; // top-left
				int V2 = IndexMap[(X + 1) + Y * Width]; // top-right
				int V3 = IndexMap[X + (Y + 1) * Width]; // bottom-left
				int V4 = IndexMap[(X + 1) + (Y + 1) * Width]; // bottom-right

				OutTriangles.Add(FIndex3i(V1, V3, V2));
				OutTriangles.Add(FIndex3i(V2, V3, V4));
			}
		}
	}

	return true;
}

bool LandscapeUtils::CreateMeshFromHeightmap(int Width, int Height, const TArray<float>& Heightmap, const FVector2D & TopLeftCorner, const FVector2D & BottomRightCorner, double ZScale, FDynamicMesh3 &OutMesh)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("CreateMeshFromHeightmap");
	

	OutMesh.Clear();


	TArray<FVector> Vertices;
	TArray<FIndex3i> Triangles;

	if (!CreateMeshFromHeightmap(Width, Height, Heightmap, TopLeftCorner, BottomRightCorner, ZScale, Vertices, Triangles))
	{
		return false;
	}

	for (auto &Vertex : Vertices)
	{
		OutMesh.AppendVertex(Vertex);
	}


	for (auto &Triangle : Triangles)
	{
		OutMesh.AppendTriangle(Triangle);
	}

	return true;
}


bool LandscapeUtils::GetLandscapeCRSBounds(ALandscape *Landscape, FVector4d &OutCoordinates)
{
	if (!IsValid(Landscape))
	{
		LCReporter::ShowError(
			LOCTEXT("LandscapeUtils::GetLandscapeCRSBounds::0", "Invalid Landscape")
		);
		return false;
	}

	UGlobalCoordinates *GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(Landscape->GetWorld());
	if (!IsValid(GlobalCoordinates)) return false;

	return GetLandscapeCRSBounds(Landscape, GlobalCoordinates, GlobalCoordinates->CRS, OutCoordinates);
}

bool LandscapeUtils::GetLandscapeCRSBounds(ALandscape *Landscape, FString ToCRS, FVector4d &OutCoordinates)
{
	if (!IsValid(Landscape))
	{
		LCReporter::ShowError(
			LOCTEXT("LandscapeUtils::GetLandscapeCRSBounds::0", "Invalid Landscape")
		);
		return false;
	}

	UGlobalCoordinates *GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(Landscape->GetWorld());
	if (!IsValid(GlobalCoordinates)) return false;

	return GetLandscapeCRSBounds(Landscape, GlobalCoordinates, ToCRS, OutCoordinates);
}

bool LandscapeUtils::GetLandscapeCRSBounds(ALandscape *Landscape, UGlobalCoordinates *GlobalCoordinates, FString ToCRS, FVector4d &OutCoordinates)
{
	if (!IsValid(Landscape))
	{
		LCReporter::ShowError(
			LOCTEXT("LandscapeUtils::GetLandscapeCRSBounds::0", "Invalid Landscape")
		);
		return false;
	}

	if (!IsValid(GlobalCoordinates))
	{
		LCReporter::ShowError(
			LOCTEXT("LandscapeUtils::GetLandscapeCRSBounds::00", "Invalid Global Coordinates")
		);
		return false;
	}

	FVector2D MinMaxX, MinMaxY, UnusedMinMaxZ;
	if (!LandscapeUtils::GetLandscapeBounds(Landscape, MinMaxX, MinMaxY, UnusedMinMaxZ))
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("LandscapeUtils::GetLandscapeCRSBounds::1", "Could not compute bounds of Landscape {0}"),
			FText::FromString(Landscape->GetActorNameOrLabel())
		));
		return false;
	}

	FVector4d Locations;
	Locations[0] = MinMaxX[0];
	Locations[1] = MinMaxX[1];
	Locations[2] = MinMaxY[1];
	Locations[3] = MinMaxY[0];
	if (!GlobalCoordinates->GetCRSCoordinatesFromUnrealLocations(Locations, ToCRS, OutCoordinates))
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("LandscapeUtils::GetLandscapeCRSBounds::2", "Could not compute coordinates of Landscape {0}"),
			FText::FromString(Landscape->GetActorNameOrLabel())
		));
		return false;
	}

	return true;
}


bool LandscapeUtils::GetActorCRSBounds(AActor* Actor, FVector4d &OutCoordinates)
{
	if (!IsValid(Actor))
	{
		LCReporter::ShowError(
			LOCTEXT("LandscapeUtils::GetActorCRSBounds::0", "Invalid Actor")
		);
		return false;
	}

	UGlobalCoordinates *GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(Actor->GetWorld());
	if (!IsValid(GlobalCoordinates)) return false;

	return GetActorCRSBounds(Actor, GlobalCoordinates, GlobalCoordinates->CRS, OutCoordinates);
}

bool LandscapeUtils::GetActorCRSBounds(AActor* Actor, FString ToCRS, FVector4d &OutCoordinates)
{
	if (!IsValid(Actor))
	{
		LCReporter::ShowError(
			LOCTEXT("LandscapeUtils::GetActorCRSBounds::0", "Invalid Actor")
		);
		return false;
	}

	UGlobalCoordinates *GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(Actor->GetWorld());
	if (!IsValid(GlobalCoordinates)) return false;

	return GetActorCRSBounds(Actor, GlobalCoordinates, ToCRS, OutCoordinates);
}

bool LandscapeUtils::GetActorCRSBounds(AActor* Actor, UGlobalCoordinates *GlobalCoordinates, FString ToCRS, FVector4d& OutCoordinates)
{
	if (!IsValid(Actor))
	{
		LCReporter::ShowError(LOCTEXT("LandscapeUtils::GetActorCRSBounds::0", "Invalid Actor"));
		return false;
	}
	else if (!IsValid(GlobalCoordinates))
	{
		LCReporter::ShowError(LOCTEXT("LandscapeUtils::GetActorCRSBounds::00", "Invalid Global Coordinates"));
		return false;
	}
	else if (Actor->IsA<ALandscape>())
	{
		return GetLandscapeCRSBounds(Cast<ALandscape>(Actor), GlobalCoordinates, ToCRS, OutCoordinates);
	}
	else
	{
		FVector Origin, BoxExtent;
		Actor->GetActorBounds(true, Origin, BoxExtent);
		if (!GlobalCoordinates->GetCRSCoordinatesFromOriginExtent(Origin, BoxExtent, ToCRS, OutCoordinates))
		{
			LCReporter::ShowError(FText::Format(
				LOCTEXT("LandscapeUtils::GetActorCRSBounds", "Internal error while reading {0}'s coordinates."),
				FText::FromString(Actor->GetActorNameOrLabel())
			));
			return false;
		}
		return true;
	}
}

#if WITH_EDITOR

#include "Editor.h"
#include "EditorModes.h"
#include "EditorModeManager.h"
#include "LandscapeEditorObject.h"
#include "LandscapeImportHelper.h" 
#include "LandscapeSubsystem.h"
#include "LandscapeUtils.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "Editor/LandscapeEditor/Private/LandscapeEdMode.h"

bool LandscapeUtils::SpawnLandscape(
	bool bIgnoreHeightmapData,
	TArray<FString> Heightmaps, FString LandscapeLabel, bool bCreateLandscapeStreamingProxies,
	bool bAutoComponents, bool bDropEdge,
	int QuadsPerSubsection0, int SectionsPerComponent0, FIntPoint ComponentCount0,
	TObjectPtr<ALandscape> &OutSpawnedLandscape, TArray<TSoftObjectPtr<ALandscapeStreamingProxy>> &OutSpawnedLandscapeStreamingProxies
)
{
	if (Heightmaps.IsEmpty())
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("SpawnLandscapeError", "Landscape Combinator Error: Cannot spawn landscape {0} without heightmaps."),
			FText::FromString(LandscapeLabel)
		));
		return false;
	}

	FString HeightmapsString = FString::Join(Heightmaps, TEXT("\n"));
	UE_LOG(LogLandscapeUtils, Log, TEXT("Importing heightmaps for Landscape %s from:\n%s"), *LandscapeLabel, *HeightmapsString);

	FString HeightmapFile = Heightmaps[0];

	// This is to prevent a failed assertion when doing `GetActiveMode`
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId("LevelEditor"));

	GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Landscape);
	FEdModeLandscape* LandscapeEdMode = (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
	ULandscapeEditorObject* UISettings = LandscapeEdMode->UISettings;
	ULandscapeSubsystem* LandscapeSubsystem = LandscapeEdMode->GetWorld()->GetSubsystem<ULandscapeSubsystem>();
	bool bIsGridBased = LandscapeSubsystem->IsGridBased();

	if (Heightmaps.Num() > 1 && !bIsGridBased)
	{
		GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Default);
		LCReporter::ShowError(
			LOCTEXT("GetHeightmapImportDescriptorError", "You must enable World Partition to be able to import multiple heightmap files.")
		);
		return false;
	}

	if (Heightmaps.Num() > 1)
	{
		check(bIsGridBased);
		FRegexPattern XYPattern(TEXT("(.*)_x\\d+_y\\d+(\\.[^.]+)"));
		FRegexMatcher XYMatcher(XYPattern, HeightmapFile);

		if (!XYMatcher.FindNext())
		{
			GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Default);
			LCReporter::ShowError(FText::Format(
				LOCTEXT("MultipleFileImportError", "Heightmap file name %s doesn't match the format: Filename_x0_y0.png."),
				FText::FromString(HeightmapFile)
			));
			return false;
		}

		HeightmapFile = XYMatcher.GetCaptureGroup(1) + XYMatcher.GetCaptureGroup(2);
		UE_LOG(LogLandscapeUtils, Log, TEXT("Detected multiple files, so we will use the base name %s to import"), *XYMatcher.GetCaptureGroup(1));
	}

	/* Load the data from the PNG files */

	TArray<uint16> Data;
	int TotalWidth, TotalHeight;

	if (bIgnoreHeightmapData)
	{
		FIntPoint ImageSize;
		if (!GDALInterface::GetPixels(ImageSize, HeightmapFile)) return false;

		TotalWidth = ImageSize.X;
		TotalHeight = ImageSize.Y;
		Data.Init(0, TotalWidth * TotalHeight);

		UE_LOG(LogLandscapeUtils, Log, TEXT("Heightmap file %s has size %dx%d"), *HeightmapFile, TotalWidth, TotalHeight);
	}
	else
	{
		FLandscapeImportDescriptor LandscapeImportDescriptor;
		FText LandscapeImportErrorMessage;

		ELandscapeImportResult DescriptorResult = FLandscapeImportHelper::GetHeightmapImportDescriptor(
			HeightmapFile,
			!bIsGridBased,
			false,
			LandscapeImportDescriptor,
			LandscapeImportErrorMessage
		);

		if (DescriptorResult == ELandscapeImportResult::Error)
		{
			GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Default);
			LCReporter::ShowError(FText::Format(
				LOCTEXT("GetHeightmapImportDescriptorError", "Internal Unreal Engine while getting import descriptor for file {0}:\n{1}\nPlease try to rename your Landscape/files to a simple name without numbers or punctuation."),
				FText::FromString(HeightmapFile),
				LandscapeImportErrorMessage
			));
			return false;
		}

		TotalWidth = LandscapeImportDescriptor.ImportResolutions[0].Width;
		TotalHeight = LandscapeImportDescriptor.ImportResolutions[0].Height;

		ELandscapeImportResult ImportResult = FLandscapeImportHelper::GetHeightmapImportData(LandscapeImportDescriptor, 0, Data, LandscapeImportErrorMessage);
		LandscapeImportDescriptor.Reset();

		if (ImportResult == ELandscapeImportResult::Error)
		{
			GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Default);
			LCReporter::ShowError(FText::Format(
				LOCTEXT("GetHeightmapImportDataError", "Internal Unreal Engine while importing {0}: {1}"),
				FText::FromString(HeightmapFile),
				LandscapeImportErrorMessage
			));
			return false;
		}
	}


	/* Expand the data to match components */

	int QuadsPerSubsection = 63;
	int SectionsPerComponent = 1;
	FIntPoint ComponentCount(8, 8);

	if (bAutoComponents)
	{
		FLandscapeImportHelper::ChooseBestComponentSizeForImport(TotalWidth, TotalHeight, QuadsPerSubsection, SectionsPerComponent, ComponentCount);

		if (bDropEdge)
		{
			ComponentCount[0]--;
			ComponentCount[1]--;
		}
	}
	else
	{
		QuadsPerSubsection = QuadsPerSubsection0;
		SectionsPerComponent = SectionsPerComponent0;
		ComponentCount = ComponentCount0;
	}

	int SizeX = ComponentCount.X * QuadsPerSubsection * SectionsPerComponent + 1;
	int SizeY = ComponentCount.Y * QuadsPerSubsection * SectionsPerComponent + 1;

	TArray<uint16> ExpandedData;
	FLandscapeImportResolution RequiredResolution(SizeX, SizeY);
	FLandscapeImportResolution ImportResolution(TotalWidth, TotalHeight);
	
	FLandscapeImportHelper::TransformHeightmapImportData(Data, ExpandedData, ImportResolution, RequiredResolution, ELandscapeImportTransformType::ExpandCentered);

	UE_LOG(LogLandscapeUtils, Log, TEXT("Required resolution: %d, %d"), SizeX, SizeY);
	UE_LOG(LogLandscapeUtils, Log, TEXT("Expanded data has size %d"), ExpandedData.Num());

	/* Create the landscape with the heightmap data. */

	TMap<FGuid, TArray<uint16>> ImportHeightData = { { FGuid(), ExpandedData } };
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> ImportMaterialLayerType = { { FGuid(), TArray<FLandscapeImportLayerInfo>() } };

	ALandscape* NewLandscape = LandscapeEdMode->GetWorld()->SpawnActor<ALandscape>(FVector::ZeroVector, FRotator::ZeroRotator);
	NewLandscape->SetActorScale3D(FVector(100, 100, 100));

#if UE_VERSION_OLDER_THAN(5,6,0)
	NewLandscape->Import(FGuid::NewGuid(), 0, 0, SizeX - 1, SizeY - 1, SectionsPerComponent, QuadsPerSubsection, ImportHeightData, NULL, ImportMaterialLayerType, ELandscapeImportAlphamapType::Additive);
#else
	NewLandscape->Import(FGuid::NewGuid(), 0, 0, SizeX - 1, SizeY - 1, SectionsPerComponent, QuadsPerSubsection, ImportHeightData, NULL, ImportMaterialLayerType, ELandscapeImportAlphamapType::Additive, TArrayView<const FLandscapeLayer>());
#endif

	GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Default);


	/* Create Landscape Streaming Proxies */

	OutSpawnedLandscape = NewLandscape;

	if (bCreateLandscapeStreamingProxies)
	{
		ULandscapeInfo* LandscapeInfo = NewLandscape->GetLandscapeInfo();

		OutSpawnedLandscape->Modify();
		UE_LOG(LogLandscapeUtils, Log, TEXT("Creating LandscapeStreamingProxies with World Partition Grid Size: %d"), UISettings->WorldPartitionGridSize);
		LandscapeSubsystem->ChangeGridSize(LandscapeInfo, UISettings->WorldPartitionGridSize);
		UE_LOG(LogLandscapeUtils, Log, TEXT("Finished changing grid size"));
		TArray<ALandscapeStreamingProxy*> LandscapeStreamingProxies = LandscapeUtils::GetLandscapeStreamingProxies(NewLandscape);
		UE_LOG(LogLandscapeUtils, Log, TEXT("Obtained %d Landscape Streaming Proxies"), LandscapeStreamingProxies.Num());
		OutSpawnedLandscapeStreamingProxies.Append(LandscapeStreamingProxies);
	}

	
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 6
	// this enables Edit Layers, and makes line traces from Spline Importers work without having
	// to restart the level
	OutSpawnedLandscape->ToggleCanHaveLayersContent();
#endif

	return true;
}

bool LandscapeUtils::ExtendLandscape(ALandscape *LandscapeToExtend, TArray<FString> Heightmaps)
{
	check(IsInGameThread());

	int NumFiles = Heightmaps.Num();
	FScopedSlowTask SlowTask(NumFiles, LOCTEXT("ExtendingLandscapeTask", "Overwriting Landscape..."));
	SlowTask.MakeDialog();
	for (int32 i = 0; i < NumFiles; i++)
	{
		SlowTask.EnterProgressFrame();
		UE_LOG(LogLandscapeUtils, Log, TEXT("Extending Landscape with Heightmap %d/%d: %s"), i + 1, NumFiles, *Heightmaps[i]);
		if (!ExtendLandscape(LandscapeToExtend, Heightmaps[i])) return false;
	}
	return true;
}

bool LandscapeUtils::CRSToQuadSpace(ALandscape *LandscapeToExtend, ULandscapeInfo *LandscapeInfo, FVector4d &Coordinates, double &OutMinQX, double &OutMaxQX, double &OutMinQY, double &OutMaxQY )
{
	const double CRSMinX = Coordinates[0];
	const double CRSMaxX = Coordinates[1];
	const double CRSMinY = Coordinates[2];
	const double CRSMaxY = Coordinates[3];

	FVector4d LandscapeCoordinates;
	if (!GetLandscapeCRSBounds(LandscapeToExtend, LandscapeCoordinates)) return false;

	int32 LMinQX, LMinQY, LMaxQX, LMaxQY;
	if (!LandscapeInfo->GetLandscapeExtent(LMinQX, LMinQY, LMaxQX, LMaxQY))
	{
		LCReporter::ShowError(FText::Format(
			LOCTEXT("LandscapeUtils::CRSToQuadSpace::1", "Could not compute Landscape Extent for Landscape {0}."),
			FText::FromString(LandscapeToExtend->GetActorNameOrLabel())
		));
		return false;
	}

	// CRS span of existing landscape
	const double LCRSMinX = LandscapeCoordinates[0];
	const double LCRSMaxX = LandscapeCoordinates[1];
	const double LCRSMinY = LandscapeCoordinates[2];
	const double LCRSMaxY = LandscapeCoordinates[3];

	const double LCRSSizeX = (LCRSMaxX - LCRSMinX);
	const double LCRSSizeY = (LCRSMaxY - LCRSMinY);

	// Quad span of existing landscape
	const double LQuadSizeX = LMaxQX - LMinQX;
	const double LQuadSizeY = LMaxQY - LMinQY;

	// Conversion: CRS delta -> quads
	const double QuadsPerCRSX = LQuadSizeX / LCRSSizeX;
	const double QuadsPerCRSY = LQuadSizeY / LCRSSizeY;

	// Map heightmap CRS bounds into the landscape's quad space
	OutMinQX = (CRSMinX - LCRSMinX) * QuadsPerCRSX + LMinQX;
	OutMaxQX = (CRSMaxX - LCRSMinX) * QuadsPerCRSX + LMinQX;
	OutMinQY = (LCRSMaxY - CRSMaxY) * QuadsPerCRSY + LMinQY;
	OutMaxQY = (LCRSMaxY - CRSMinY) * QuadsPerCRSY + LMinQY;

	return true;
}

bool LandscapeUtils::ExtendLandscape(ALandscape *LandscapeToExtend, FString Heightmap)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ExtendLandscape::ExtendLandscape");

#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION <= 5 && ENGINE_MINOR_VERSION < 6)
	LCReporter::ShowError(LOCTEXT("LandscapeUtils::ExtendLandscape", "Landscape Extension is only supported on Unreal Engine 5.6 and up."));
	return false;

#else
	check(IsInGameThread());

	if (!IsValid(LandscapeToExtend))
	{
		LCReporter::ShowError(LOCTEXT("LandscapeUtils::ExtendLandscape", "The Landscape to extend is invalid."));
		return false;
	}

	TObjectPtr<UGlobalCoordinates> GlobalCoordinates = ALevelCoordinates::GetGlobalCoordinates(LandscapeToExtend->GetWorld(), true);
	if (!IsValid(GlobalCoordinates)) return false;

	ULandscapeInfo *LandscapeInfo = LandscapeToExtend->GetLandscapeInfo();
	if (!IsValid(LandscapeInfo))
	{
		LCReporter::ShowError(LOCTEXT("LandscapeUtils::ExtendLandscape::1", "The Landscape to extend doesn't have a LandscapeInfo."));
		return false;
	}

	const int32 ComponentSizeQuads = LandscapeInfo->ComponentSizeQuads;
	const int32 ComponentNumSubsections = LandscapeInfo->ComponentNumSubsections;
	const int32 SubsectionSizeQuads = LandscapeInfo->SubsectionSizeQuads;

	FVector4d FileCoordinates;
	if (!GDALInterface::GetCoordinates(FileCoordinates, Heightmap)) return false;

	double FMinQX, FMaxQX, FMinQY, FMaxQY;
	if (!CRSToQuadSpace(LandscapeToExtend, LandscapeInfo, FileCoordinates, FMinQX, FMaxQX, FMinQY, FMaxQY)) return false;

	// Compute inclusive component index range covering the file bounds
	const int32 FCompX1 = FMath::FloorToInt(FMinQX / ComponentSizeQuads);
	const int32 FCompX2 = FMath::CeilToInt (FMaxQX / ComponentSizeQuads) - 1;
	const int32 FCompY1 = FMath::FloorToInt(FMinQY / ComponentSizeQuads);
	const int32 FCompY2 = FMath::CeilToInt (FMaxQY / ComponentSizeQuads) - 1;

	int LCompX1 = MAX_int32;
	int LCompX2 = MIN_int32;
	int LCompY1 = MAX_int32;
	int LCompY2 = MIN_int32;
	for (auto &[XY, Component]: LandscapeInfo->XYtoComponentMap)
	{
		LCompX1 = FMath::Min(XY.X, LCompX1);
		LCompX2 = FMath::Max(XY.X, LCompX2);
		LCompY1 = FMath::Min(XY.Y, LCompY1);
		LCompY2 = FMath::Max(XY.Y, LCompY2);
	}

	const int32 MinCompX = FMath::Min(FCompX1, LCompX1);
	const int32 MaxCompX = FMath::Max(FCompX2, LCompX2);
	const int32 MinCompY = FMath::Min(FCompY1, LCompY1);
	const int32 MaxCompY = FMath::Max(FCompY2, LCompY2);

	TSet<FIntPoint> ComponentsToAdd;
	for (int32 CompX = MinCompX; CompX <= MaxCompX; CompX++)
	{
		for (int32 CompY = MinCompY; CompY <= MaxCompY; CompY++)
		{
			const FIntPoint CompCoord(CompX, CompY);
			if (!LandscapeInfo->XYtoComponentMap.Contains(CompCoord))
			{
				ComponentsToAdd.Add(CompCoord);
			}
		}
	}

	if (ComponentsToAdd.Num() > 100 && !LCReporter::ShowMessage(
		FText::Format(
			LOCTEXT("AddManyComponents", "Extending Landscape {0} with heigthmap {1} requires adding {2} components. Continue?"),
			FText::FromString(LandscapeToExtend->GetActorNameOrLabel()),
			FText::FromString(Heightmap),
			ComponentsToAdd.Num()
		),
		"SuppressAddManyComponents"
	))
	{
		return false;
	}

	TArray<ULandscapeComponent*> AddedComponents;
	for (auto &CompCoord: ComponentsToAdd)
	{
		FIntPoint ComponentBase = CompCoord * LandscapeInfo->ComponentSizeQuads;
		ULandscapeSubsystem* LandscapeSubsystem = LandscapeToExtend->GetWorld()->GetSubsystem<ULandscapeSubsystem>();
		if (!IsValid(LandscapeSubsystem))
		{
			LCReporter::ShowError(LOCTEXT("InvalidLandscapeSubsystem", "Internal Error: Invalid landscape subsystem"));
			return false;
		}

		ALandscapeProxy* LandscapeProxy = LandscapeSubsystem->FindOrAddLandscapeProxy(LandscapeInfo, ComponentBase);
		if (!IsValid(LandscapeProxy))
		{
			LCReporter::ShowError(LOCTEXT("InvalidLandscapeProxy", "Internal Error: Invalid landscape proxy"));
			return false;
		}

		ULandscapeComponent *LandscapeComponent = NewObject<ULandscapeComponent>(LandscapeProxy, NAME_None, RF_Transactional);
		if (!IsValid(LandscapeComponent))
		{
			LCReporter::ShowError(LOCTEXT("InvalidLandscapeComponent", "Internal Error: Could not create landscape component"));
			return false;
		}

		AddedComponents.Add(LandscapeComponent);

		{
			TRACE_CPUPROFILER_EVENT_SCOPE_STR("ExtendLandscape::LandscapeComponent::Init");
			LandscapeComponent->Init(
				ComponentBase.X, ComponentBase.Y,
				ComponentSizeQuads,
				ComponentNumSubsections,
				SubsectionSizeQuads
			);
		}

		TArray<FColor> HeightData;
		const int32 ComponentVerts = (LandscapeComponent->SubsectionSizeQuads + 1) * LandscapeComponent->NumSubsections;
		HeightData.AddZeroed(FMath::Square(ComponentVerts));
		
		{
			TRACE_CPUPROFILER_EVENT_SCOPE_STR("ExtendLandscape::InitHeightmapData");
			LandscapeComponent->InitHeightmapData(HeightData, true);
		}
		{
			TRACE_CPUPROFILER_EVENT_SCOPE_STR("ExtendLandscape::UpdateMaterialInstances");
			LandscapeComponent->UpdateMaterialInstances();
		}

		LandscapeInfo->XYtoComponentMap.Add(CompCoord, LandscapeComponent);
		LandscapeInfo->XYtoAddCollisionMap.Remove(CompCoord);

		UE_LOG(LogTemp, Log, TEXT("Added new component at (%d,%d)"), CompCoord.X, CompCoord.Y);
	}

	for (auto &LandscapeComponent: AddedComponents)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("ExtendLandscape::RegisterComponent");
		LandscapeComponent->RegisterComponent();
	}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 6
	if (LandscapeToExtend->HasLayersContent())
	{
		LandscapeToExtend->RequestLayersInitialization();
	}
#else
	LandscapeToExtend->RequestLayersInitialization();
#endif

	for (auto &LandscapeComponent: AddedComponents)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_STR("ExtendLandscape::Updates");

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION <= 6
		if (LandscapeToExtend->HasLayersContent())
#else
		if (true)
#endif
		{
			TArray<ULandscapeComponent*> ComponentsUsingHeightmap;
			ComponentsUsingHeightmap.Add(LandscapeComponent);
			for (const ULandscapeEditLayerBase* EditLayer : LandscapeToExtend->GetEditLayersConst())
			{
				TMap<UTexture2D*, UTexture2D*> CreatedHeightmapTextures;
				LandscapeComponent->AddDefaultLayerData(EditLayer->GetGuid(), ComponentsUsingHeightmap, CreatedHeightmapTextures);
			}
		}
		LandscapeComponent->UpdateCachedBounds();
		LandscapeComponent->UpdateBounds();
		LandscapeComponent->MarkRenderStateDirty();
	}

	// recompute the file bounds in quad space after the new components have been added
	double FMinQX2, FMaxQX2, FMinQY2, FMaxQY2;
	if (!CRSToQuadSpace(LandscapeToExtend, LandscapeInfo, FileCoordinates, FMinQX2, FMaxQX2, FMinQY2, FMaxQY2)) return false;

	int32 TotalSizeX = LandscapeToExtend->ComputeComponentCounts().X * ComponentSizeQuads + 1;
	int32 TotalSizeY = LandscapeToExtend->ComputeComponentCounts().Y * ComponentSizeQuads + 1;

	FMinQX2 = (int) FMath::Min(TotalSizeX - 1, FMath::Max(0, FMath::FloorToInt(FMinQX2)));
	FMaxQX2 = (int) FMath::Min(TotalSizeX - 1, FMath::Max(0, FMath::CeilToInt(FMaxQX2)));
	FMinQY2 = (int) FMath::Min(TotalSizeY - 1, FMath::Max(0, FMath::FloorToInt(FMinQY2)));
	FMaxQY2 = (int) FMath::Min(TotalSizeY - 1, FMath::Max(0, FMath::CeilToInt(FMaxQY2)));

	int32 SizeX = FMaxQX2 - FMinQX2 + 1;
	int32 SizeY = FMaxQY2 - FMinQY2 + 1;

	if (SizeX > INT32_MAX / SizeY)
	{
		LCReporter::ShowError(
			LOCTEXT("LandscapeUtils::ExtendLandscape::LargeArea", "The size of the area to edit is too large.")
		);
		return false;
	}
	
	uint16* HeightmapDataUE = (uint16*) malloc(SizeX * SizeY * (sizeof (uint16)));
	if (!HeightmapDataUE)
	{
		LCReporter::ShowError(
			LOCTEXT("LandscapeUtils::ExtendLandscape::NotEnoughMemory", "Not enough memory to allocate for new heightmap data.")
		);
		return false;
	}

	int OutWidth, OutHeight;
	TArray<float> HeightmapMeters;
	if (!GDALInterface::ReadHeightmapFromFile(Heightmap, OutWidth, OutHeight, HeightmapMeters)) return false;

	// fill the HeightmapDataUE with the data from the heightmap, taking into account the fact
	// that they don't have the same resolution (bilinear interpolation)
	for (int32 Y = 0; Y < SizeY; Y++)
	{
		for (int32 X = 0; X < SizeX; X++)
		{
			const float SourceX = ((float) X) * (OutWidth  - 1) / (SizeX - 1);
			const float SourceY = ((float) Y) * (OutHeight - 1) / (SizeY - 1);

			const int X0 = FMath::FloorToInt(SourceX);
			const int Y0 = FMath::FloorToInt(SourceY);
			const int X1 = FMath::Min(X0 + 1, OutWidth  - 1);
			const int Y1 = FMath::Min(Y0 + 1, OutHeight - 1);

			const float Dx = SourceX - X0;
			const float Dy = SourceY - Y0;

			// Bilinear interpolation
			const float H00 = HeightmapMeters[Y0 * OutWidth + X0];
			const float H10 = HeightmapMeters[Y0 * OutWidth + X1];
			const float H01 = HeightmapMeters[Y1 * OutWidth + X0];
			const float H11 = HeightmapMeters[Y1 * OutWidth + X1];

			const float GlobalHeightMeters =
				(1 - Dx) * (1 - Dy) * H00 +
				Dx       * (1 - Dy) * H10 +
				(1 - Dx) * Dy       * H01 +
				Dx       * Dy       * H11;

			const float LocalHeight = (GlobalHeightMeters * 100.0f - LandscapeToExtend->GetActorLocation().Z) / LandscapeToExtend->GetActorScale3D().Z;
			HeightmapDataUE[Y * SizeX + X] = LandscapeDataAccess::GetTexHeight(LocalHeight);
		}
	}
	
	FHeightmapAccessor<false> HeightmapAccessor(LandscapeInfo);
	
	HeightmapAccessor.SetEditLayer(LandscapeToExtend->GetEditLayer(0)->GetGuid());	
	HeightmapAccessor.SetData(FMinQX2, FMinQY2, FMaxQX2, FMaxQY2, HeightmapDataUE);
	free(HeightmapDataUE);

	return true;
#endif
}


#endif

#undef LOCTEXT_NAMESPACE
