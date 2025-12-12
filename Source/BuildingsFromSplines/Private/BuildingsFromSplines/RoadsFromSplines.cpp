
// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "BuildingsFromSplines/RoadsFromSplines.h"

#include "OSMUserData/OSMUserData.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "LandscapeUtils/LandscapeUtils.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConcurrencyHelpers/LCReporter.h"

#include "Components/SplineMeshComponent.h"
#include "Logging/StructuredLog.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/MessageDialog.h"
#include "TransactionCommon.h" 
#include "Engine/World.h"
#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RoadsFromSplines)

#define LOCTEXT_NAMESPACE "FBuildingsFromSplinesModule"

ARoadsFromSplines::ARoadsFromSplines()
{
	PrimaryActorTick.bCanEverTick = false;
	
	EmptySceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("EmptySceneComponent"));
	EmptySceneComponent->SetMobility(EComponentMobility::Static);
	RootComponent = EmptySceneComponent;
}

bool ARoadsFromSplines::OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated)
{
	return GenerateRoads(bIsUserInitiated);
}

bool ARoadsFromSplines::Cleanup_Implementation(bool bSkipPrompt)
{
	Modify();
	
	if (DeleteGeneratedObjects(bSkipPrompt))
	{
		SplineMeshComponents.Empty();
		AlreadyHandledSplines.Empty();
		return true;
	}
	else
	{
		return false;
	}
}

void ARoadsFromSplines::ClearRoads()
{
	Execute_Cleanup(this, false);
}

bool ARoadsFromSplines::GenerateRoads(bool bIsUserInitiated)
{
	Modify();

	if (bDeleteOldRoadsWhenCreatingRoads)
	{
		if (!Execute_Cleanup(this, !bIsUserInitiated)) return false;
	}

	UWorld *World = GetWorld();
	TSet<TObjectPtr<USplineComponent>> SplineComponents;
	
	Concurrency::RunOnGameThreadAndWait([&]() {
		SplineComponents = ULCBlueprintLibrary::FindSplineComponents(World, bIsUserInitiated, SplinesTag, SplineComponentsTag);
		return true;
	});
	const int FoundComponents = SplineComponents.Num();
	SplineComponents = SplineComponents.Difference(AlreadyHandledSplines);
	const int RemainingComponents = SplineComponents.Num();
	UE_LOG(LogBuildingsFromSplines, Log, TEXT("Found %d spline components, %d already handled, %d remaining to generate"), FoundComponents, AlreadyHandledSplines.Num(), RemainingComponents);

	FCollisionQueryParams CollisionQueryParams;
	if (bAdaptSplineMeshRollToLandscape)
	{
		if (!Concurrency::RunOnGameThreadAndWait([&]() {
			return LandscapeUtils::CustomCollisionQueryParams(World, LandscapeSelection, CollisionQueryParams);
		}))
		{
			return false;
		}
	}

	for (TObjectPtr<USplineComponent> SplineComponent: SplineComponents)
	{
		if (!IsValid(SplineComponent)) continue;
	
		AlreadyHandledSplines.Add(SplineComponent);

		const int32 NumPoints = SplineComponent->GetNumberOfSplinePoints();

		TArray<FVector> LandscapeNormals;
		if (bAdaptSplineMeshRollToLandscape)
		{
			LandscapeNormals.SetNum(NumPoints);
			for (int32 i = 0; i < NumPoints; i++)
			{
				FVector PointLocationWorld = SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
				FVector StartLocation = FVector(PointLocationWorld.X, PointLocationWorld.Y, HALF_WORLD_MAX);
				FVector EndLocation = FVector(PointLocationWorld.X, PointLocationWorld.Y, -HALF_WORLD_MAX);

				FHitResult HitResult;
				bool bLineTrace = World->LineTraceSingleByChannel(
					OUT HitResult,
					StartLocation,
					EndLocation,
					ECollisionChannel::ECC_Visibility,
					CollisionQueryParams
				);

				if (bLineTrace) LandscapeNormals[i] = HitResult.Normal;
			}
		}

		for (int32 i = 0; i < NumPoints-1; i++)
		{
			FVector StartWorld = SplineComponent->GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
			FVector StartTangentWorld = SplineComponent->GetTangentAtSplinePoint(i, ESplineCoordinateSpace::World);
			FVector EndWorld = SplineComponent->GetLocationAtSplinePoint(i + 1, ESplineCoordinateSpace::World);
			FVector EndTangentWorld = SplineComponent->GetTangentAtSplinePoint(i + 1, ESplineCoordinateSpace::World);

			const FTransform& ParentTransform = GetActorTransform();
			FVector StartLocal = ParentTransform.InverseTransformPosition(StartWorld);
			FVector StartTangentLocal = ParentTransform.InverseTransformVector(StartTangentWorld);
			FVector EndLocal = ParentTransform.InverseTransformPosition(EndWorld);
			FVector EndTangentLocal = ParentTransform.InverseTransformVector(EndTangentWorld);
			StartLocal.Z += ZOffset;
			EndLocal.Z += ZOffset;

			if (!Concurrency::RunOnGameThreadAndWait([&]() {
				if (!IsValid(GetWorld()) || !IsValid(RootComponent)) return false;
				USplineMeshComponent* SplineMeshComponent = NewObject<USplineMeshComponent>(RootComponent);
				if (!IsValid(SplineMeshComponent)) return false;
				SplineMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
				SplineMeshComponent->SetStaticMesh(RoadMesh);
				SplineMeshComponent->SetForwardAxis(RoadMeshAxis, false);
				SplineMeshComponent->SetStartAndEnd(StartLocal, StartTangentLocal, EndLocal, EndTangentLocal, false);
				SplineMeshComponent->SetStartScale(FVector2D(WidthScale, HeightScale), false);
				SplineMeshComponent->SetEndScale(FVector2D(WidthScale, HeightScale), false);

				if (bAdaptSplineMeshRollToLandscape)
				{
					if (!LandscapeNormals[i].IsZero())
					{
						float MaxRoll = FMath::Acos(LandscapeNormals[i].Z);
						FVector TiltAxis = FVector::CrossProduct(LandscapeNormals[i], FVector::UpVector).GetSafeNormal();
						float Factor = FVector::DotProduct(StartTangentWorld.GetSafeNormal(), TiltAxis);
						SplineMeshComponent->SetStartRoll(MaxRoll * Factor, false);
					}

					if (!LandscapeNormals[i+1].IsZero())
					{
						float MaxRoll = FMath::Acos(LandscapeNormals[i+1].Z);
						FVector TiltAxis = FVector::CrossProduct(LandscapeNormals[i+1], FVector::UpVector).GetSafeNormal();
						float Factor = FVector::DotProduct(EndTangentWorld.GetSafeNormal(), TiltAxis);
						SplineMeshComponent->SetEndRoll(MaxRoll * Factor, false);
					}
				}

				SplineMeshComponent->SetCollisionProfileName("BlockAll");
				SplineMeshComponent->MarkRenderStateDirty();
				SplineMeshComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
				SplineMeshComponent->RegisterComponent();
				AddInstanceComponent(SplineMeshComponent);
				SplineMeshComponents.Add(SplineMeshComponent);
				
				OnSplineMeshCreated(SplineComponent, SplineMeshComponent);
				return true;
			}))
			{
				return false;
			}
		}
	}

	return true;
}


#if WITH_EDITOR

AActor *ARoadsFromSplines::Duplicate(FName FromName, FName ToName)
{
	if (ARoadsFromSplines *NewRoadsFromSplines =
		Cast<ARoadsFromSplines>(GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->DuplicateActor(this)))
	{
		NewRoadsFromSplines->SplinesTag = ULCBlueprintLibrary::ReplaceName(SplinesTag, FromName, ToName);
		NewRoadsFromSplines->SplineComponentsTag = ULCBlueprintLibrary::ReplaceName(SplineComponentsTag, FromName, ToName);

		return NewRoadsFromSplines;
	}
	else
	{
		LCReporter::ShowError(LOCTEXT("ARoadsFromSplines::DuplicateActor", "Failed to duplicate actor."));
		return nullptr;
	}
}

#endif