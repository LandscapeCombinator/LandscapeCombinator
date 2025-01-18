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
	InteriorMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_White.MI_Flat_White'")));
	ExteriorMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_Wall_NoDecal.MI_Flat_Wall_NoDecal'")));
	FloorMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_Floor.MI_Flat_Floor'")));
	CeilingMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_White.MI_Flat_White'")));
	RoofMaterial = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *FString("/Script/Engine.MaterialInstanceConstant'/LandscapeCombinator/Materials/MI_Flat_Wall_NoDecal.MI_Flat_Wall_NoDecal'")));
}

bool FLevelDescription::GetWallSegments(TArray<FWallSegment*> &OutWallSegments, UDataTable *WallSegmentsTable, double TargetSize) const
{
	auto FetchSegment = [WallSegmentsTable](const FName &Id) -> FWallSegment*
	{
		auto *Row = WallSegmentsTable->FindRow<FWallSegment>(Id, TEXT("GetWallSegments"));
		if (!Row)
		{
			ULCReporter::ShowError(FText::Format(
				LOCTEXT("GetWallSegmentsError", "Invalid wall segment Id: '{0}'"),
				{ FText::FromString(Id.ToString()) }
			));
		}
		return Row;
	};

	return UBuildingConfiguration::Expand<FWallSegment>(
		OutWallSegments,
		WallSegmentsIds,
		FetchSegment,
		[](FWallSegment* Segment) { return Segment ? Segment->GetReferenceSize() : 0;},
		TargetSize
	);
}

TArray<FName> UBuildingConfiguration::GetWallSegmentNames()
{
	TArray<FName> Result;
	Result.Add("BeginRepeat");
	Result.Add("EndRepeat");

// this list is only used in editor
#if WITH_EDITOR
	UDataTable *WallSegmentsTable = LoadObject<UDataTable>(nullptr, TEXT("/Script/Engine.DataTable'/LandscapeCombinator/Buildings/DT_WallSegments.DT_WallSegments'"));
	ADataTablesOverride* DataTablesOverride = 
		Cast<ADataTablesOverride>(UGameplayStatics::GetActorOfClass(
			GEditor->GetEditorWorldContext().World(),
			ADataTablesOverride::StaticClass()
		));
	if (IsValid(DataTablesOverride) && IsValid(DataTablesOverride->WallSegmentsTable))
	{
		WallSegmentsTable = DataTablesOverride->WallSegmentsTable;
	}

	if (IsValid(WallSegmentsTable))
		Result.Append(WallSegmentsTable->GetRowNames());
#endif

	return Result;
}

TArray<FName> UBuildingConfiguration::GetLevelNames()
{
	TArray<FName> Result;
	Result.Add("BeginRepeat");
	Result.Add("EndRepeat");

// this list is only used in editor
#if WITH_EDITOR
	UDataTable *LevelsTable = LoadObject<UDataTable>(nullptr, TEXT("/Script/Engine.DataTable'/LandscapeCombinator/Buildings/DT_Levels.DT_Levels'"));
	ADataTablesOverride* DataTablesOverride = 
		Cast<ADataTablesOverride>(UGameplayStatics::GetActorOfClass(
			GEditor->GetEditorWorldContext().World(),
			ADataTablesOverride::StaticClass()
		));
	if (IsValid(DataTablesOverride) && IsValid(DataTablesOverride->LevelsTable))
	{
		LevelsTable = DataTablesOverride->LevelsTable;
	}

	if (IsValid(LevelsTable))
		Result.Append(LevelsTable->GetRowNames());
#endif

	return Result;
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
		FString LevelsString = BuildingOSMUserData->Fields["levels"];
		int NumLevels = FCString::Atoi(*LevelsString);
		if (NumLevels > 0)
		{
			Building->BuildingConfiguration->bUseRandomNumFloors = false;
			Building->BuildingConfiguration->NumFloors = NumLevels;
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
