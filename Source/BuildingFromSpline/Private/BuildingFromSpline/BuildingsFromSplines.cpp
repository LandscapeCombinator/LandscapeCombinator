
// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "BuildingFromSpline/BuildingsFromSplines.h"

#include "OSMUserData/OSMUserData.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "LCReporter/LCReporter.h"
#include "ConcurrencyHelpers/Concurrency.h"

#include "Components/SplineMeshComponent.h"
#include "Logging/StructuredLog.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/MessageDialog.h"
#include "TransactionCommon.h" 
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BuildingsFromSplines)

#define LOCTEXT_NAMESPACE "FBuildingFromSplineModule"

ABuildingsFromSplines::ABuildingsFromSplines()
{
	PrimaryActorTick.bCanEverTick = false;

	BuildingConfiguration = CreateDefaultSubobject<UBuildingConfiguration>(TEXT("Building Configuration"));

	BuildingConfiguration->BuildingGeometry = EBuildingGeometry::BuildingWithoutInside;
	BuildingConfiguration->InternalWallThickness = 1;
	BuildingConfiguration->ExternalWallThickness = 1;
	BuildingConfiguration->bBuildInternalWalls = false;
	BuildingConfiguration->bAutoPadWallBottom = true;
}

TArray<AActor*> ABuildingsFromSplines::FindActors()
{
	TArray<AActor*> Actors;

	Concurrency::RunOnGameThreadAndWait([&Actors, this]()
	{
		if (SplinesTag.IsNone())
		{	
			UGameplayStatics::GetAllActorsOfClass(this->GetWorld(), AActor::StaticClass(), Actors);
		}
		else
		{
			UGameplayStatics::GetAllActorsWithTag(this->GetWorld(), SplinesTag, Actors);
		}
	});

	return Actors;
}

TArray<USplineComponent*> ABuildingsFromSplines::FindSplineComponents(bool bIsUserInitiated)
{
	TArray<USplineComponent*> Result;

	bool bFound = false;
	
	for (auto& Actor : FindActors())
	{
		if (SplineComponentsTag.IsNone())
		{
			TArray<USplineComponent*> SplineComponents;
			Actor->GetComponents<USplineComponent>(SplineComponents, true);

			for (auto &SplineComponent : SplineComponents)
			{
				Result.Add(Cast<USplineComponent>(SplineComponent));
			}
		}
		else
		{
			for (auto &SplineComponent : Actor->GetComponentsByTag(USplineComponent::StaticClass(), SplineComponentsTag))
			{
				Result.Add(Cast<USplineComponent>(SplineComponent));
			}
		}
	}

	if (bIsUserInitiated && Result.Num() == 0)
	{
		ULCReporter::ShowError(
			LOCTEXT("NoSplineComponentTagged", "BuildingsFromSplines: Could not find spline components with the given tags.")
		);
	}
	
	return Result;
}

void ABuildingsFromSplines::OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated, TFunction<void(bool)> OnComplete)
{
	Concurrency::RunOnGameThreadAndWait([&SpawnedActorsPathOverride, bIsUserInitiated, this](){
		GenerateBuildings(SpawnedActorsPathOverride, bIsUserInitiated);
	});
	OnComplete(true);
}

bool ABuildingsFromSplines::Cleanup_Implementation(bool bSkipPrompt)
{
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

void ABuildingsFromSplines::GenerateBuildings(FName SpawnedActorsPathOverride, bool bIsUserInitiated)
{
	if (bDeleteOldBuildingsWhenCreatingBuildings)
	{
		if (!Execute_Cleanup(this, false)) return;
	}

	TArray<USplineComponent*> SplineComponents = FindSplineComponents(bIsUserInitiated);
	const int NumComponents = SplineComponents.Num();
	UE_LOG(LogBuildingFromSpline, Log, TEXT("Found %d spline components to create buildings"), NumComponents);
	FScopedSlowTask GenerateTask = FScopedSlowTask(NumComponents,
		FText::Format(
			LOCTEXT("GenerateTask", "Generating {0} Buildings"),
			FText::AsNumber(NumComponents)
		)
	);
	
	if (bIsUserInitiated)
	{
		GenerateTask.MakeDialog(true);
	}

	for (int i = 0; i < NumComponents; i++)
	{
		if (bIsUserInitiated && GenerateTask.ShouldCancel()) break;
		if (bIsUserInitiated) GenerateTask.EnterProgressFrame(1);
	
		if (bSkipExistingBuildings && ProcessedSplines.Contains(SplineComponents[i])) continue;
		
		if (GenerateBuilding(SplineComponents[i], SpawnedActorsPathOverride, bIsUserInitiated))
			ProcessedSplines.Add(SplineComponents[i]);
		else if (!bContinueDespiteErrors)
			break;
	}

#if WITH_EDITOR
	Concurrency::RunOnGameThread([](){
		if (GEditor) GEditor->NoteSelectionChange(); // to avoid folders being in rename mode
	});
#endif
}

void ABuildingsFromSplines::ClearBuildings()
{
	Execute_Cleanup(this, false);
}

bool ABuildingsFromSplines::GenerateBuilding(USplineComponent* SplineComponent, FName SpawnedActorsPathOverride, bool bIsUserInitiated)
{
	int NumPoints = SplineComponent->GetNumberOfSplinePoints();
	if (NumPoints < 2) return false;

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

	ABuilding* Building = GetWorld()->SpawnActor<ABuilding>(Location, RotatorForLargestSegment);

#if WITH_EDITOR
	ULCBlueprintLibrary::SetFolderPath2(Building, SpawnedActorsPathOverride, SpawnedActorsPath);
#endif

	Buildings.Add(Building);

	Building->BuildingConfiguration = NewObject<UBuildingConfiguration>(
		Building, UBuildingConfiguration::StaticClass(), "DuplicatedBuildingConfiguration", RF_NoFlags, BuildingConfiguration
	);

	Building->SplineComponent->ClearSplinePoints();
	for (int i = 0; i < NumPoints; i++)
	{
		Building->SplineComponent->AddSplinePoint(SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World), ESplineCoordinateSpace::World, false);
		Building->SplineComponent->SetSplinePointType(i, SplineComponent->GetSplinePointType(i), false);
	}
	Building->SplineComponent->UpdateSpline();
	
	UOSMUserData *SplineOSMUserData = Cast<UOSMUserData>(SplineComponent->GetAssetUserDataOfClass(UOSMUserData::StaticClass()));

	if (SplineOSMUserData && Building->GetRootComponent())
	{
		UOSMUserData *BuildingOSMUserData = NewObject<UOSMUserData>(Building->GetRootComponent());
		BuildingOSMUserData->Fields = SplineOSMUserData->Fields;
		Building->GetRootComponent()->AddAssetUserData(BuildingOSMUserData);
	}

#if WITH_EDITOR
	Building->SetIsSpatiallyLoaded(bBuildingsSpatiallyLoaded);
#endif

	return Building->GenerateBuilding_Internal(SpawnedActorsPathOverride);
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
		ULCReporter::ShowError(LOCTEXT("ABuildingsFromSplines::DuplicateActor", "Failed to duplicate actor."));
		return nullptr;
	}
}

#endif

#undef LOCTEXT_NAMESPACE
