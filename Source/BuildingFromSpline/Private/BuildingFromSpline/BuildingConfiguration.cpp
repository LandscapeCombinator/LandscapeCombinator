// Copyright 2023 LandscapeCombinator. All Rights Reserved.

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
	Materials.Add(Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_Wall_NoDecal.MI_Flat_Wall_NoDecal'"))));
	Materials.Add(Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_Floor.MI_Flat_Floor'"))));
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
