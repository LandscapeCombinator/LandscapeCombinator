
// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "BuildingsFromSplines/BuildingsFromSplines.h"

#include "OSMUserData/OSMUserData.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Components/SplineMeshComponent.h"
#include "Logging/StructuredLog.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/MessageDialog.h"
#include "TransactionCommon.h" 
#include "Engine/World.h"
#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BuildingsFromSplines)

#define LOCTEXT_NAMESPACE "FBuildingsFromSplinesModule"

TObjectPtr<UBuildingConfiguration> FWeightedBuildingConfiguration::GetRandomBuildingConfiguration(TArray<FWeightedBuildingConfiguration>& BuildingConfigurations)
{
	int Index = ULCBlueprintLibrary::GetRandomIndex<FWeightedBuildingConfiguration>(BuildingConfigurations);
	if (0 <= Index && Index < BuildingConfigurations.Num()) return BuildingConfigurations[Index].BuildingConfiguration;
	else return nullptr;
}

ABuildingsFromSplines::ABuildingsFromSplines()
{
	PrimaryActorTick.bCanEverTick = false;

	FWeightedBuildingConfiguration WBCFG;
	WBCFG.BuildingConfiguration = CreateDefaultSubobject<UBuildingConfiguration>(TEXT("BuildingConfiguration"));
	BuildingConfigurations.Add(MoveTemp(WBCFG));
}

bool ABuildingsFromSplines::OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated)
{
	return Concurrency::RunOnGameThreadAndWait([this, SpawnedActorsPathOverride, bIsUserInitiated]()
	{
		return GenerateBuildings(SpawnedActorsPathOverride, bIsUserInitiated);
	});
}

bool ABuildingsFromSplines::Cleanup_Implementation(bool bSkipPrompt)
{
	Modify();
	
	if (DeleteGeneratedObjects(bSkipPrompt))
	{
		Buildings.Empty();
		ProcessedSplines.Empty();
		return true;
	}
	else
	{
		return false;
	}
}

bool ABuildingsFromSplines::GenerateBuildings(FName SpawnedActorsPathOverride, bool bIsUserInitiated)
{
	Modify();

	if (!IsInGameThread())
	{
		LCReporter::ShowError(
			LOCTEXT("NotInGameThread", "BuildingsFromSplines: GenerateBuildings must be called on the game thread.")
		);

		return false;
	}

	if (BuildingConfigurations.IsEmpty())
	{
		LCReporter::ShowError(
			LOCTEXT("NoBuildingConfiguration", "BuildingsFromSplines: No Building Configurations.")
		);
		return false;
	}

	if (bDeleteOldBuildingsWhenCreatingBuildings)
	{
		if (!Execute_Cleanup(this, !bIsUserInitiated)) return false;
	}

	TArray<USplineComponent*> SplineComponents = ULCBlueprintLibrary::FindSplineComponents(GetWorld(), bIsUserInitiated, SplinesTag, SplineComponentsTag);
	const int NumComponents = SplineComponents.Num();
	UE_LOG(LogBuildingsFromSplines, Log, TEXT("Found %d spline components to create buildings"), NumComponents);

	for (USplineComponent* SplineComponent : SplineComponents)
	{
		if (bSkipExistingBuildings && ProcessedSplines.Contains(SplineComponent)) continue;

		if (GenerateBuilding(SplineComponent, SpawnedActorsPathOverride, bIsUserInitiated))
			ProcessedSplines.Add(SplineComponent);
		else if (!bContinueDespiteErrors)
			return false;
	}


#if WITH_EDITOR
	if (GEditor) GEditor->NoteSelectionChange(); // to avoid folders being in rename mode
#endif

	return true;
}

void ABuildingsFromSplines::ClearBuildings()
{
	Execute_Cleanup(this, false);
}

bool ABuildingsFromSplines::GenerateBuilding(USplineComponent* SplineComponent, FName SpawnedActorsPathOverride, bool bIsUserInitiated)
{
	int NumPoints = SplineComponent->GetNumberOfSplinePoints();
	if (NumPoints <= 1)
	{
		UE_LOG(LogBuildingsFromSplines, Log, TEXT("Skipping building with zero or one point"));
		return true;
	}

	FVector Location = SplineComponent->GetWorldLocationAtSplinePoint(0);

	/* Determine the rotation of the longest segment */
	/* This allows us to set the actor rotation, and we can then use Local Position */
	/* in the floor and roof material to have tiles that go "straight" */

	double MaxDistance = 0;
	FRotator RotatorForLargestSegment = FRotator();
	int MaxDistanceIndex = 0;
	for (int i = 0; i < NumPoints; i++)
	{
		FVector Point1 = SplineComponent->GetWorldLocationAtSplinePoint(i);
		FVector Point2 = SplineComponent->GetWorldLocationAtSplinePoint((i + 1) % NumPoints);
		Point1[2] = 0;
		Point2[2] = 0;
		double Distance = FVector::Distance(Point1, Point2);
		if (Distance > MaxDistance)
		{
			MaxDistance = Distance;
			MaxDistanceIndex = i;
			RotatorForLargestSegment = FQuat::FindBetweenVectors(FVector(1, 0, 0), Point2 - Point1).Rotator();
		}
	}

	ABuilding* Building = nullptr;
	
	bool bSuccess = Concurrency::RunOnGameThreadAndWait([this, &Building, &Location, &RotatorForLargestSegment, &SpawnedActorsPathOverride, NumPoints, SplineComponent]() -> bool
	{
		Building = GetWorld()->SpawnActor<ABuilding>(Location, RotatorForLargestSegment);
		if (!IsValid(Building))
		{
			LCReporter::ShowError(
				LOCTEXT("CannotSpawnBuilding", "Internal Error: Cannot Spawn Building")
			);
			return false;
		}

#if WITH_EDITOR
		ULCBlueprintLibrary::SetFolderPath2(Building, SpawnedActorsPathOverride, SpawnedActorsPath);
#endif

		Buildings.Add(Building);
        Building->BCfg = FWeightedBuildingConfiguration::GetRandomBuildingConfiguration(BuildingConfigurations);

		Building->SplineComponent->ClearSplinePoints();
		for (int i = 0; i < NumPoints; i++)
		{
			Building->SplineComponent->AddSplinePoint(SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World), ESplineCoordinateSpace::World, false);
			Building->SplineComponent->SetSplinePointType(i, SplineComponent->GetSplinePointType(i), false);
		}
		Building->SplineComponent->UpdateSpline();
		
		UOSMUserData *SplineOSMUserData = Cast<UOSMUserData>(SplineComponent->GetAssetUserDataOfClass(UOSMUserData::StaticClass()));

		if (IsValid(SplineOSMUserData) && Building->GetRootComponent())
		{
			UOSMUserData *BuildingOSMUserData = NewObject<UOSMUserData>(Building->GetRootComponent());
			BuildingOSMUserData->Fields = SplineOSMUserData->Fields;
			Building->GetRootComponent()->AddAssetUserData(BuildingOSMUserData);
		}

#if WITH_EDITOR
		Building->SetIsSpatiallyLoaded(bBuildingsSpatiallyLoaded);
#endif
		return true;
	});

	if (bSuccess) return Building->GenerateBuilding_Internal(SpawnedActorsPathOverride);
	else return false;
}

#if WITH_EDITOR

AActor *ABuildingsFromSplines::Duplicate(FName FromName, FName ToName)
{
	if (ABuildingsFromSplines *NewBuildingsFromSplines =
		Cast<ABuildingsFromSplines>(GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->DuplicateActor(this)))
	{
		NewBuildingsFromSplines->SplinesTag = ULCBlueprintLibrary::ReplaceName(SplinesTag, FromName, ToName);
		NewBuildingsFromSplines->SplineComponentsTag = ULCBlueprintLibrary::ReplaceName(SplineComponentsTag, FromName, ToName);

		return NewBuildingsFromSplines;
	}
	else
	{
		LCReporter::ShowError(LOCTEXT("ABuildingsFromSplines::DuplicateActor", "Failed to duplicate actor."));
		return nullptr;
	}
}

#endif

#undef LOCTEXT_NAMESPACE
