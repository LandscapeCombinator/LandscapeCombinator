// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeUtils/LandscapeUtils.h"
#include "LandscapeUtils/LogLandscapeUtils.h"
#include "LCReporter/LCReporter.h"

#include "Kismet/KismetMathLibrary.h"
#include "Internationalization/Regex.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/MessageDialog.h"
#include "Engine/World.h"
    

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
			ULCReporter::ShowError(FText::Format(
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
	UGameplayStatics::GetAllActorsOfClass(Landscape->GetWorld(), ALandscapeStreamingProxy::StaticClass(), LandscapeStreamingProxiesTemp);

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
		ULCReporter::ShowError(LOCTEXT("InvalidCollindingActor", "Invalid colliding actor"));
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
		ULCReporter::ShowError(LOCTEXT("InvalidCollidingActors", "Invalid colliding actors for custom collision query"));
		return false;
	}

	TSet<AActor*> CollidingActorsSet = TSet<AActor*>(CollidingActors);
	for (auto &CollidingActor: CollidingActors)
	{
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

	if (!IsValid(World))
	{
		ULCReporter::ShowError(LOCTEXT("InvalidWorld", "Invalid world for custom collision query"));
		return false;
	}
	
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


#if WITH_EDITOR

#include "Editor.h"
#include "EditorModes.h"
#include "EditorModeManager.h"
#include "LandscapeEditorObject.h"
#include "LandscapeImportHelper.h" 
#include "LandscapeSubsystem.h"
#include "LandscapeUtils.h"

bool LandscapeUtils::SpawnLandscape(
	TArray<FString> Heightmaps, FString LandscapeLabel, bool bCreateLandscapeStreamingProxies,
	bool bAutoComponents, bool bDropData,
	int QuadsPerSubsection0, int SectionsPerComponent0, FIntPoint ComponentCount0,
	ALandscape* &OutSpawnedLandscape, TArray<TSoftObjectPtr<ALandscapeStreamingProxy>> &OutSpawnedLandscapeStreamingProxies
)
{
	if (Heightmaps.IsEmpty())
	{
		ULCReporter::ShowError(FText::Format(
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
		ULCReporter::ShowError(
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
			ULCReporter::ShowError(FText::Format(
				LOCTEXT("MultipleFileImportError", "Heightmap file name %s doesn't match the format: Filename_x0_y0.png."),
				FText::FromString(HeightmapFile)
			));
			return false;
		}

		HeightmapFile = XYMatcher.GetCaptureGroup(1) + XYMatcher.GetCaptureGroup(2);
		UE_LOG(LogLandscapeUtils, Log, TEXT("Detected multiple files, so we will use the base name %s to import"), *XYMatcher.GetCaptureGroup(1));
	}

	/* Load the data from the PNG files */

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
		ULCReporter::ShowError(FText::Format(
			LOCTEXT("GetHeightmapImportDescriptorError", "Internal Unreal Engine while getting import descriptor for file {0}:\n{1}\nPlease try to rename your Landscape/files to a simple name without numbers or punctuation."),
			FText::FromString(HeightmapFile),
			LandscapeImportErrorMessage
		));
		return false;
	}

	int TotalWidth = LandscapeImportDescriptor.ImportResolutions[0].Width;
	int TotalHeight = LandscapeImportDescriptor.ImportResolutions[0].Height;

	TArray<uint16> Data;
	ELandscapeImportResult ImportResult = FLandscapeImportHelper::GetHeightmapImportData(LandscapeImportDescriptor, 0, Data, LandscapeImportErrorMessage);

	LandscapeImportDescriptor.Reset();

	if (ImportResult == ELandscapeImportResult::Error)
	{
		GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Default);
		ULCReporter::ShowError(FText::Format(
			LOCTEXT("GetHeightmapImportDataError", "Internal Unreal Engine while importing {0}: {1}"),
			FText::FromString(HeightmapFile),
			LandscapeImportErrorMessage
		));
		return false;
	}

	/* Expand the data to match components */

	int QuadsPerSubsection = 63;
	int SectionsPerComponent = 1;
	FIntPoint ComponentCount(8, 8);

	if (bAutoComponents)
	{
		FLandscapeImportHelper::ChooseBestComponentSizeForImport(TotalWidth, TotalHeight, QuadsPerSubsection, SectionsPerComponent, ComponentCount);

		if (bDropData)
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


	/* Import the landscape */

	TMap<FGuid, TArray<uint16>> ImportHeightData = { { FGuid(), ExpandedData } };
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> ImportMaterialLayerType = { { FGuid(), TArray<FLandscapeImportLayerInfo>() } };

	ALandscape* NewLandscape = LandscapeEdMode->GetWorld()->SpawnActor<ALandscape>(FVector::ZeroVector, FRotator::ZeroRotator);
	NewLandscape->SetActorScale3D(FVector(100, 100, 100));
	NewLandscape->Import(FGuid::NewGuid(), 0, 0, SizeX - 1, SizeY - 1, SectionsPerComponent, QuadsPerSubsection, ImportHeightData, NULL, ImportMaterialLayerType, ELandscapeImportAlphamapType::Additive);

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

	// this enables Edit Layers, and makes line traces from Spline Importers work without having
	// to restart the level
	OutSpawnedLandscape->ToggleCanHaveLayersContent();

	return true;
}

#endif

#undef LOCTEXT_NAMESPACE
