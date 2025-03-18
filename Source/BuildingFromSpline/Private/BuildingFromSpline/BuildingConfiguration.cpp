// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "BuildingFromSpline/BuildingConfiguration.h"
#include "BuildingFromSpline/BuildingsFromSplines.h"
#include "BuildingFromSpline/DataTablesOverride.h"

#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

#include "Materials/MaterialInterface.h"
#include "OSMUserData/OSMUserData.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif

#define LOCTEXT_NAMESPACE "FBuildingFromSplineModule"

UBuildingConfiguration::UBuildingConfiguration()
{
	Materials.Add(Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_White.MI_Flat_White'"))));
	Materials.Add(Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_Wall.MI_Flat_Wall'"))));
	Materials.Add(Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_Floor.MI_Flat_Floor'"))));

	FLevelDescription LevelDescription;
	LevelDescription.LevelHeight = 300.0;
	LevelDescription.FloorThickness = 20.0;
	LevelDescription.FloorMaterialIndex = 0;
	LevelDescription.UnderFloorMaterialIndex = 0;
	LevelDescription.bResetWallSegmentsOnCorners = true;

	FWallSegment WallSegment;
	WallSegment.WallSegmentKind = EWallSegmentKind::Wall;
	WallSegment.bAutoExpand = true;
	WallSegment.SegmentLength = 500.0;

	FWallSegment HoleSegment;
	HoleSegment.WallSegmentKind = EWallSegmentKind::Hole;
	FAttachment Attachment;
	Attachment.Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *FString("/Script/Engine.StaticMesh'/LandscapeCombinator/Meshes/SM_Window.SM_Window'")));
	Attachment.Offset = FVector(0.0, 0.0, 100.0);
	Attachment.OverrideWidth = 100.0;
	Attachment.OverrideHeight = 100.0;
	HoleSegment.Attachments.Add(Attachment);

	LevelDescription.WallSegments.Add(WallSegment);
	LevelDescription.WallSegments.Add(HoleSegment);
	LevelDescription.WallSegments.Add(WallSegment);

	FLoop SegmentsLoop;
	SegmentsLoop.StartIndex = 0;
	SegmentsLoop.EndIndex = 1;
	LevelDescription.WallSegmentLoops.Add(SegmentsLoop);
	Levels.Add(LevelDescription);

	FLoop LevelLoop;
	LevelLoop.StartIndex = 0;
	LevelLoop.EndIndex = 0;
	LevelLoops.Add(LevelLoop);
}

bool UBuildingConfiguration::OwnerIsBFS()
{
	return GetOwner() && GetOwner()->IsA<ABuildingsFromSplines>();
}

bool UBuildingConfiguration::AutoComputeNumFloors()
{
	if (!bAutoComputeNumFloors) return false;

	ABuilding *Building = Cast<ABuilding>(GetOwner());

	if (!Building || !Building->GetRootComponent()) return false;

	UOSMUserData *BuildingOSMUserData = Cast<UOSMUserData>(Building->GetRootComponent()->GetAssetUserDataOfClass(UOSMUserData::StaticClass()));

	if (!BuildingOSMUserData) return false;
	
	if (BuildingOSMUserData->Fields.Contains("building_levels"))
	{
		FString LevelsString = BuildingOSMUserData->Fields["building_levels"];
		int NumLevels = FCString::Atoi(*LevelsString);
		if (NumLevels > 0)
		{
			Building->BuildingConfiguration->bUseRandomNumFloors = false;
			Building->BuildingConfiguration->NumFloors = NumLevels;
			return true;
		}
		// we don't return false to give a chance to the 'height' field below
		else if (!LevelsString.IsEmpty())
		{
			UE_LOG(LogBuildingFromSpline, Warning, TEXT("Ignoring levels field: '%s'"), *LevelsString);
		}
	}

	if (BuildingOSMUserData->Fields.Contains("height"))
	{
		FString HeightString = BuildingOSMUserData->Fields["height"];
		double Height = FCString::Atod(*HeightString);
		if (Height > 0)
		{
			Building->BuildingConfiguration->bUseRandomNumFloors = false;
			Building->BuildingConfiguration->NumFloors = FMath::Max(1, Height * 100 / 300);
			return true;
		}
		else if (!HeightString.IsEmpty())
		{
			UE_LOG(LogBuildingFromSpline, Warning, TEXT("Ignoring height field: '%s'"), *HeightString)
			return false;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

#undef LOCTEXT_NAMESPACE
