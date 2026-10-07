// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "BuildingsFromSplines/BuildingConfiguration.h"
#include "BuildingsFromSplines/BuildingsFromSplines.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "LCCommon/Expression.h"

#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

#include "Materials/MaterialInterface.h"
#include "OSMUserData/OSMUserData.h"

#define LOCTEXT_NAMESPACE "FBuildingsFromSplinesModule"

UBuildingConfiguration::UBuildingConfiguration()
{
}

int UBuildingConfiguration::ResolveMaterial(FString ExprStr)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("ResolveMaterial");

	FExpression *Expr = FExpression::Parse(ExprStr);
	if (!Expr) return 0;

	if (Expr->ExprType != EExprType::Concat) return 0;
	
	Expr->MakeChoices();
	if (Expr->Children.IsEmpty()) return 0;

	int Index = MaterialNamesArray.IndexOfByKey(Expr->Children[0]->Symbol);
	if (Index >= 0) return Index;
	else return 0;
}

bool UBuildingConfiguration::AutoComputeNumFloors(UOSMUserData *BuildingOSMUserData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("AutoComputeNumFloors");

	if (!bAutoComputeNumFloors) return false;
	if (!IsValid(BuildingOSMUserData)) return false;
	
	if (BuildingOSMUserData->Fields.Contains(LevelsTag))
	{
		FString LevelsString = BuildingOSMUserData->Fields[LevelsTag];
		int NumLevels = FCString::Atoi(*LevelsString);
		if (NumLevels > 0)
		{
			bUseRandomNumFloors = false;
			NumFloors = NumLevels;
			return true;
		}
		// we don't return false to give a chance to the 'height' field below
		else if (!LevelsString.IsEmpty())
		{
			UE_LOG(LogBuildingsFromSplines, Warning, TEXT("Ignoring levels field: '%s'"), *LevelsString);
		}
	}

	if (BuildingOSMUserData->Fields.Contains(HeightTag))
	{
		FString HeightString = BuildingOSMUserData->Fields[HeightTag];
		double Height = FCString::Atod(*HeightString);
		if (Height > 0)
		{
			bUseRandomNumFloors = false;
			NumFloors = FMath::Max(1, Height * 100 / 300);
			return true;
		}
		else if (!HeightString.IsEmpty())
		{
			UE_LOG(LogBuildingsFromSplines, Warning, TEXT("Ignoring height field: '%s'"), *HeightString);
		}
	}

	return false;
}

bool UBuildingConfiguration::RequireLevel(const FString& Key) const
{
	if (HasLevel(Key)) return true;

	LCReporter::ShowError(FText::Format(
		LOCTEXT("UnknownLevel", "Unknown Level: '{0}'. Please adjust your expression."),
		FText::FromString(Key)
	));
	return false;
}

ULevelDescription* UBuildingConfiguration::GetLevel(const FString& Key) const
{
	return AssetLink::Resolve(LevelsMap.FindRef(Key).Get());
}

UWallSegment* ULevelDescription::GetSegment(const FString& Key) const
{
	return AssetLink::Resolve(WallSegmentsMap.FindRef(Key).Get());
}

bool ULevelDescription::RequireSegment(const FString& Key, const FString& LevelKey) const
{
	if (HasSegment(Key)) return true;

	LCReporter::ShowError(FText::Format(
		LOCTEXT("UnknownWallSegment", "Unknown WallSegment: '{0}' in level '{1}'. Please adjust your expression, openings or filler."),
		FText::FromString(Key), FText::FromString(LevelKey)
	));
	return false;
}

#undef LOCTEXT_NAMESPACE
