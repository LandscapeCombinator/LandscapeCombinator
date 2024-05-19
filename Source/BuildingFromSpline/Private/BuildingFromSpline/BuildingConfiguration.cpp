// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "BuildingFromSpline/BuildingConfiguration.h"
#include "BuildingFromSpline/BuildingsFromSplines.h"

#include "Materials/MaterialInterface.h"
#include "OSMUserData/OSMUserData.h"

UBuildingConfiguration::UBuildingConfiguration()
{
	InteriorMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_White.MI_Flat_White'")));
	ExteriorMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_Wall_NoDecal.MI_Flat_Wall_NoDecal'")));
	FloorMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_Floor.MI_Flat_Floor'")));
	CeilingMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_White.MI_Flat_White'")));
	RoofMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_Wall_NoDecal.MI_Flat_Wall_NoDecal'")));

	FWindowsSpecification GroundWindowsSpecification, OtherWindowsSpecification;

	GroundWindowsSpecification.MinDistanceWindowToCorner = 150;
	GroundWindowsSpecification.MinDistanceWindowToWindow = 450;
	GroundWindowsSpecification.WindowsWidth = 240;
	GroundWindowsSpecification.WindowsHeight = 210;
	GroundWindowsSpecification.WindowsDistanceToFloor = 5;

	OtherWindowsSpecification.MinDistanceWindowToCorner = 150;
	OtherWindowsSpecification.MinDistanceWindowToWindow = 350;
	OtherWindowsSpecification.WindowsWidth = 140;
	OtherWindowsSpecification.WindowsHeight = 140;
	OtherWindowsSpecification.WindowsDistanceToFloor = 110;

	WindowsSpecifications.Add(GroundWindowsSpecification);
	WindowsSpecifications.Add(OtherWindowsSpecification);
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
	
	if (BuildingOSMUserData->Fields.Contains("levels"))
	{
		FString LevelsString = BuildingOSMUserData->Fields["height"];
		int Levels = FCString::Atoi(*LevelsString);
		if (Levels > 0)
		{
			Building->BuildingConfiguration->bUseRandomNumFloors = false;
			Building->BuildingConfiguration->NumFloors = Levels;
			return true;
		}
		else if (!LevelsString.IsEmpty())
		{
			UE_LOG(LogBuildingFromSpline, Warning, TEXT("Ignoring levels field: '%s'"), *LevelsString);
			return false;
		}
		else
		{
			return false;
		}
	}
	else if (BuildingOSMUserData->Fields.Contains("height"))
	{
		FString HeightString = BuildingOSMUserData->Fields["height"];
		double Height = FCString::Atod(*HeightString);
		if (Height > 0)
		{
			Building->BuildingConfiguration->bUseRandomNumFloors = false;
			Building->BuildingConfiguration->NumFloors = FMath::Max(1, Height * 100 / Building->BuildingConfiguration->FloorHeight);
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
