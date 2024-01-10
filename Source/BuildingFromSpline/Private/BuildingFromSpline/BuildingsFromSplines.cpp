
// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "BuildingFromSpline/BuildingsFromSplines.h"

#include "OSMUserData/OSMUserData.h"

#include "Components/SplineMeshComponent.h"
#include "Logging/StructuredLog.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopedSlowTask.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ObjectWriter.h"
#include "TransactionCommon.h" 

#include UE_INLINE_GENERATED_CPP_BY_NAME(BuildingsFromSplines)

#define LOCTEXT_NAMESPACE "FBuildingFromSplineModule"

ABuildingsFromSplines::ABuildingsFromSplines()
{
	PrimaryActorTick.bCanEverTick = false;

	BuildingConfiguration = CreateDefaultSubobject<UBuildingConfiguration>(TEXT("Building Configuration"));

	BuildingConfiguration->BuildingKind = EBuildingKind::Simple;
	BuildingConfiguration->InternalWallThickness = 1;
	BuildingConfiguration->ExternalWallThickness = 1;
	BuildingConfiguration->bBuildInternalWalls = false;
}

#if WITH_EDITOR

TArray<AActor*> ABuildingsFromSplines::FindActors()
{
	TArray<AActor*> Actors;

	if (ActorsTag.IsNone())
	{
		UGameplayStatics::GetAllActorsOfClass(this->GetWorld(), AActor::StaticClass(), Actors);
	}
	else
	{
		UGameplayStatics::GetAllActorsWithTag(this->GetWorld(), ActorsTag, Actors);
	}

	return Actors;
}

TArray<USplineComponent*> ABuildingsFromSplines::FindSplineComponents()
{
	TArray<USplineComponent*> Result;

	bool bFound = false;
	
	for (auto& Actor : FindActors())
	{
		if (SplinesTag.IsNone())
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
			for (auto &SplineComponent : Actor->GetComponentsByTag(USplineComponent::StaticClass(), SplinesTag))
			{
				Result.Add(Cast<USplineComponent>(SplineComponent));
			}
		}
	}

	if (Result.Num() == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			LOCTEXT("NoSplineComponentTagged", "Could not find spline components with the given tags.")
		);
	}
	
	return Result;
}

void ABuildingsFromSplines::GenerateBuildings()
{
	ClearBuildings();

	TArray<USplineComponent*> SplineComponents = FindSplineComponents();
	const int NumComponents = SplineComponents.Num();
	FScopedSlowTask GenerateTask = FScopedSlowTask(NumComponents,
		FText::Format(
			LOCTEXT("GenerateTask", "Generating {0} Buildings"),
			FText::AsNumber(NumComponents)
		)
	);
	GenerateTask.MakeDialog(true);

	for (int i = 0; i < NumComponents; i++)
	{
		if (GenerateTask.ShouldCancel())
		{
			break;
		}
		GenerateTask.EnterProgressFrame(1);
		GenerateBuilding(SplineComponents[i]);
	}
}

void ABuildingsFromSplines::ClearBuildings()
{
	for (auto& Building : SpawnedBuildings)
	{
		if (IsValid(Building))
		{
			Building->Destroy();
		}
	}
	SpawnedBuildings.Reset();
}

void ABuildingsFromSplines::GenerateBuilding(USplineComponent* SplineComponent)
{
	int NumPoints = SplineComponent->GetNumberOfSplinePoints();
	if (NumPoints < 2) return;

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
	Building->SetFolderPath(FName(*(FString("/") + GetActorLabel())));
	SpawnedBuildings.Add(Building);

	TArray<uint8> BufferData;
	FMemoryWriter BufferArchive(BufferData);
	FObjectAndNameAsStringProxyArchive Writer = FObjectAndNameAsStringProxyArchive(BufferArchive, true);
	BuildingConfiguration->Serialize(Writer);

	FMemoryReader BufferReader = FMemoryReader(BufferData, true);
	FObjectAndNameAsStringProxyArchive Reader = FObjectAndNameAsStringProxyArchive(BufferReader, true);
	Building->BuildingConfiguration->Serialize(Reader);
	// enums are not properly serialized, so we copy them ourselves
	Building->BuildingConfiguration->BuildingKind = BuildingConfiguration->BuildingKind;
	Building->BuildingConfiguration->RoofKind = BuildingConfiguration->RoofKind;

	Building->SplineComponent->ClearSplinePoints();
	for (int i = 0; i < NumPoints; i++)
	{
		Building->SplineComponent->AddSplinePoint(SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World), ESplineCoordinateSpace::World, false);
		Building->SplineComponent->SetSplinePointType(i, SplineComponent->GetSplinePointType(i), false);
	}
	Building->SplineComponent->UpdateSpline();

	Building->ComputeMinMaxHeight();

	if (bAutoComputeWallBottom)
	{
		Building->BuildingConfiguration->ExtraWallBottom = Building->MaxHeightLocal - Building->MinHeightLocal + 100;
	}
	
	UOSMUserData *SplineOSMUserData = Cast<UOSMUserData>(SplineComponent->GetAssetUserDataOfClass(UOSMUserData::StaticClass()));

	if (bAutoComputeNumFloors)
	{
		if (SplineOSMUserData && SplineOSMUserData->Fields.Contains("height"))
		{
			FString HeightString = SplineOSMUserData->Fields["height"];
			double Height = FCString::Atod(*HeightString);
			if (Height > 0)
			{
				Building->BuildingConfiguration->bUseRandomNumFloors = false;
				Building->BuildingConfiguration->NumFloors = FMath::Max(1, Height * 100 / Building->BuildingConfiguration->FloorHeight);
			}
			else if (!HeightString.IsEmpty())
			{
				UE_LOG(LogBuildingFromSpline, Warning, TEXT("Ignoring height field: '%s'"), *HeightString)
			}
		}
	}

	UOSMUserData *BuildingOSMUserData = NewObject<UOSMUserData>(Building->GetRootComponent());
	BuildingOSMUserData->Fields = SplineOSMUserData->Fields;
	Building->GetRootComponent()->AddAssetUserData(BuildingOSMUserData);
	
	Building->SetReceivesDecals(bBuildingsReceiveDecals);
	Building->SetIsSpatiallyLoaded(bBuildingsSpatiallyLoaded);

	Building->GenerateBuilding();
}

#endif

#undef LOCTEXT_NAMESPACE
