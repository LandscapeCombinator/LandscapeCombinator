// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "BuildingFromSpline/BuildingConfiguration.h"
#include "BuildingFromSpline/BuildingsFromSplines.h"
#include "BuildingFromSpline/DataTablesOverride.h"

#include "Kismet/GameplayStatics.h"

#include "Materials/MaterialInterface.h"
#include "OSMUserData/OSMUserData.h"

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

	return Result;
}

TArray<FName> UBuildingConfiguration::GetLevelNames()
{
	TArray<FName> Result;
	Result.Add("BeginRepeat");
	Result.Add("EndRepeat");

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

template<class T>
static bool UBuildingConfiguration::Expand(
	TArray<T*> &OutItems,
	const TArray<FName> Ids,
	TFunction<T*(FName)> GetItem,
	TFunction<double(T*)> GetSize,
	double TargetSize
)
{
	OutItems.Empty();
	if (!AreValidIds(Ids, GetItem)) return false;

	TArray<int> LoopStarts;
	TArray<int> LoopEnds;
	TArray<double> LoopSizes;
	bool bInsideLoop = false;
	int ArraySize = Ids.Num();
	double CurrentLoopSize = 0;
	double OverallSizes = 0; // initialized with items that are outside loops

	// first pass on the array to find loops and their sizes
	for (int IdIndex = 0; IdIndex < ArraySize; IdIndex++)
	{
		const FName &Id = Ids[IdIndex];
		if (Id == "BeginRepeat")
		{
			LoopStarts.Add(IdIndex);
			bInsideLoop = true;
		}
		else if (Id == "EndRepeat")
		{
			LoopEnds.Add(IdIndex);
			LoopSizes.Add(CurrentLoopSize);
			bInsideLoop = false;
			CurrentLoopSize = 0;
		}
		else
		{
			T *Item = GetItem(Id);
			// should never happen because we check `AreValidIds` at the beginning of the function
			if (!Item) return false;

			if (bInsideLoop) CurrentLoopSize += GetSize(Item);
			else OverallSizes += GetSize(Item);
		}
	}

	// if we are already above the threshold, return an empty array
	if (OverallSizes > TargetSize) return true;

	// compute how many times each loop can be unrolled, in a fair way
	TArray<int> LoopUnrollCounts;
	LoopUnrollCounts.Init(0, LoopSizes.Num());
	while (true)
	{
		bool bUnrolled = false;

		for (int LoopIndex = 0; LoopIndex < LoopSizes.Num(); LoopIndex++)
		{
			if (OverallSizes + LoopSizes[LoopIndex] <= TargetSize)
			{
				if (LoopSizes[LoopIndex] <= 0)
				{
					UE_LOG(LogBuildingFromSpline, Warning, TEXT("Invalid loop size: %d, continuing anyway"), LoopSizes[LoopIndex]);
					continue;
				}
				LoopUnrollCounts[LoopIndex]++;
				OverallSizes += LoopSizes[LoopIndex];
				bUnrolled = true;
			}
		}
		
		if (!bUnrolled) break;
	}

	// second pass on the loop to unroll the loops
	int IdIndex = 0;
	int LoopIndex = 0;
	while (IdIndex < ArraySize)
	{
		const FName &Id = Ids[IdIndex];
		if (Id == "BeginRepeat")
		{
			int LoopEnd = LoopEnds[LoopIndex];
			int UnrollCount = LoopUnrollCounts[LoopIndex];

			for (int i = 0; i < UnrollCount; i++)
			{
				for (int j = IdIndex + 1; j < LoopEnd; j++)
				{
					T* Item = GetItem(Ids[j]);
					if (Item) OutItems.Add(Item);
				}
			}

			IdIndex = LoopEnd + 1; // Skip to the end of the loop
			LoopIndex++;
		}
		else
		{
			// these are items outside any loop
			T* Item = GetItem(Id);
			if (Item) OutItems.Add(Item);
			IdIndex += 1;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
