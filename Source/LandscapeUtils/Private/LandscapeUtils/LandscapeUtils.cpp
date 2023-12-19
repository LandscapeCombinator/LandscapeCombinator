// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeUtils/LandscapeUtils.h"
#include "LandscapeUtils/LogLandscapeUtils.h"

#include "Internationalization/Regex.h"
#include "Editor.h"
#include "EditorModes.h"
#include "EditorModeManager.h"
#include "Kismet/GameplayStatics.h"
#include "LandscapeEditorObject.h"
#include "LandscapeImportHelper.h" 
#include "LandscapeSubsystem.h"

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

ALandscape* LandscapeUtils::SpawnLandscape(
	TArray<FString> Heightmaps, FString LandscapeLabel, bool bCreateLandscapeStreamingProxies,
	bool bAutoComponents, bool bDropData,
	int QuadsPerSubsection0, int SectionsPerComponent0, FIntPoint ComponentCount0
)
{
	if (Heightmaps.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("SpawnLandscapeError", "Landscape Combinator Error: Cannot spawn landscape {0} without heightmaps."),
			FText::FromString(LandscapeLabel)
		));
		return NULL;
	}

	FString HeightmapsString = FString::Join(Heightmaps,TEXT("\n"));
	UE_LOG(LogLandscapeUtils, Log, TEXT("Importing heightmaps for Landscape %s from:\n%s"), *LandscapeLabel, *HeightmapsString);

	FString HeightmapFile = Heightmaps[0];

	// This is to prevent a failed assertion when doing `GetActiveMode`
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId("LevelEditor"));
	
	GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Landscape);
	FEdModeLandscape* LandscapeEdMode = (FEdModeLandscape*) GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
	ULandscapeEditorObject* UISettings = LandscapeEdMode->UISettings;
	ULandscapeSubsystem* LandscapeSubsystem = LandscapeEdMode->GetWorld()->GetSubsystem<ULandscapeSubsystem>();
	bool bIsGridBased = LandscapeSubsystem->IsGridBased();

	if (Heightmaps.Num() > 1 && !bIsGridBased)
	{
		GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Default);
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("GetHeightmapImportDescriptorError", "You must enable World Partition to be able to import multiple heightmap files.")
		);
		return NULL;
	}

	if (bIsGridBased && Heightmaps.Num() > 1)
	{
		FRegexPattern XYPattern(TEXT("(.*)_x\\d+_y\\d+(\\.[^.]+)"));
		FRegexMatcher XYMatcher(XYPattern, HeightmapFile);

		if (!XYMatcher.FindNext())
		{
			GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Default);
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
				LOCTEXT("MultipleFileImportError", "Heightmap file name %s doesn't match the format: Filename_x0_y0.png."),
				FText::FromString(HeightmapFile)
			));
			return NULL;
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
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("GetHeightmapImportDescriptorError", "Internal Unreal Engine while getting import descriptor for file {0}:\n{1}\nPlease try to rename your Landscape/files to a simple name without numbers or punctuation."),
			FText::FromString(HeightmapFile),
			LandscapeImportErrorMessage
		));
		return NULL;
	}

	int TotalWidth  = LandscapeImportDescriptor.ImportResolutions[0].Width;
	int TotalHeight = LandscapeImportDescriptor.ImportResolutions[0].Height;
	
	TArray<uint16> Data;
	ELandscapeImportResult ImportResult = FLandscapeImportHelper::GetHeightmapImportData(LandscapeImportDescriptor, 0, Data, LandscapeImportErrorMessage);
	
	LandscapeImportDescriptor.Reset();

	if (ImportResult == ELandscapeImportResult::Error)
	{
		GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Default);
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
			LOCTEXT("GetHeightmapImportDataError", "Internal Unreal Engine while importing {0}: {1}"),
			FText::FromString(HeightmapFile),
			LandscapeImportErrorMessage
		));
		return NULL;
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
	TMap<FGuid, TArray<FLandscapeImportLayerInfo>> ImportMaterialLayerType = { { FGuid(), TArray<FLandscapeImportLayerInfo>() }};

	ALandscape* NewLandscape = LandscapeEdMode->GetWorld()->SpawnActor<ALandscape>(FVector::ZeroVector, FRotator::ZeroRotator);
	NewLandscape->SetActorScale3D(FVector(100, 100, 100));
	NewLandscape->Import(FGuid::NewGuid(), 0, 0, SizeX - 1, SizeY - 1, SectionsPerComponent, QuadsPerSubsection, ImportHeightData, NULL, ImportMaterialLayerType, ELandscapeImportAlphamapType::Additive);
	
	GLevelEditorModeTools().ActivateMode(FBuiltinEditorModes::EM_Default);
	

	/* Create Landscape Streaming Proxies */
	
	if (bCreateLandscapeStreamingProxies)
	{
		ULandscapeInfo* LandscapeInfo = NewLandscape->GetLandscapeInfo();
		LandscapeSubsystem->ChangeGridSize(LandscapeInfo, UISettings->WorldPartitionGridSize);
	}

	return NewLandscape;
}

bool LandscapeUtils::GetLandscapeBounds(ALandscape *Landscape, TArray<ALandscapeStreamingProxy*> LandscapeStreamingProxies, FVector2D &MinMaxX, FVector2D &MinMaxY, FVector2D &MinMaxZ)
{
	FString LandscapeLabel = Landscape->GetActorLabel();

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
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
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
	UWorld* World = GEditor->GetEditorWorldContext().World();

	TArray<AActor*> LandscapeStreamingProxiesTemp;
	UGameplayStatics::GetAllActorsOfClass(World, ALandscapeStreamingProxy::StaticClass(), LandscapeStreamingProxiesTemp);

	TArray<ALandscapeStreamingProxy*> LandscapeStreamingProxies;

	for (auto& LandscapeStreamingProxy0 : LandscapeStreamingProxiesTemp)
	{
		ALandscapeStreamingProxy* LandscapeStreamingProxy = Cast<ALandscapeStreamingProxy>(LandscapeStreamingProxy0);
		if (LandscapeStreamingProxy->GetLandscapeActor() == Landscape)
			LandscapeStreamingProxies.Add(LandscapeStreamingProxy);
	}

	return LandscapeStreamingProxies;
}

ALandscape* LandscapeUtils::GetLandscapeFromLabel(FString LandscapeLabel)
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	TArray<AActor*> Landscapes;
	UGameplayStatics::GetAllActorsOfClass(World, ALandscape::StaticClass(), Landscapes);

	for (auto& Landscape0 : Landscapes)
	{
		if (Landscape0->GetActorLabel().Equals(LandscapeLabel))
		{
			return Cast<ALandscape>(Landscape0); 
		}
	}

	return NULL;
}

// Parameters to collide with this actor only, ignoring all other actors
FCollisionQueryParams LandscapeUtils::CustomCollisionQueryParams(AActor *Actor)
{
	UWorld *World = Actor->GetWorld();
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
	FCollisionQueryParams CollisionQueryParams;

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

	return CollisionQueryParams;
}

bool LandscapeUtils::GetZ(UWorld* World, FCollisionQueryParams CollisionQueryParams, double x, double y, double &OutZ)
{
	FVector StartLocation = FVector(x, y, HALF_WORLD_MAX);
	FVector EndLocation = FVector(x, y, -HALF_WORLD_MAX);

	FHitResult HitResult;
	World->LineTraceSingleByObjectType(
		OUT HitResult,
		StartLocation,
		EndLocation,
		FCollisionObjectQueryParams(ECollisionChannel::ECC_WorldStatic),
		CollisionQueryParams
	);

	if (HitResult.GetActor())
	{
		OutZ = HitResult.ImpactPoint.Z;
		return true;
	}
	else
	{
		return false;
	}
}

#undef LOCTEXT_NAMESPACE
