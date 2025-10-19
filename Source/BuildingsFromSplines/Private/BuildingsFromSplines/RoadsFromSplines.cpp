
// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "BuildingsFromSplines/RoadsFromSplines.h"

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
	return Concurrency::RunOnGameThreadAndWait([this, SpawnedActorsPathOverride, bIsUserInitiated]()
	{	
		return GenerateRoads(bIsUserInitiated);
	});
}

bool ARoadsFromSplines::Cleanup_Implementation(bool bSkipPrompt)
{
	Modify();
	
	if (DeleteGeneratedObjects(bSkipPrompt))
	{
		SplineMeshComponents.Empty();
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

	if (!IsInGameThread())
	{
		LCReporter::ShowError(
			LOCTEXT("NotInGameThread", "ARoadsFromSplines: Generate Roads must be called on the game thread.")
		);

		return false;
	}

	if (bDeleteOldRoadsWhenCreatingRoads)
	{
		if (!Execute_Cleanup(this, !bIsUserInitiated)) return false;
	}

	TArray<USplineComponent*> SplineComponents = ULCBlueprintLibrary::FindSplineComponents(GetWorld(), bIsUserInitiated, SplinesTag, SplineComponentsTag);
	const int NumComponents = SplineComponents.Num();
	UE_LOG(LogBuildingsFromSplines, Log, TEXT("Found %d spline components to create roads"), NumComponents);

	for (USplineComponent* SplineComponent : SplineComponents)
	{
		if (!IsValid(SplineComponent)) continue;
		const int32 NumPoints = SplineComponent->GetNumberOfSplinePoints();

		for (int32 i = 0; i < NumPoints - 1; i++)
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

			USplineMeshComponent* SplineMeshComponent = NewObject<USplineMeshComponent>(RootComponent);
			SplineMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
			SplineMeshComponent->SetStaticMesh(RoadMesh);
			SplineMeshComponent->SetForwardAxis(RoadMeshAxis, false);
			SplineMeshComponent->SetStartAndEnd(StartLocal, StartTangentLocal, EndLocal, EndTangentLocal, false);
			SplineMeshComponent->SetStartScale(FVector2D(WidthScale, HeightScale), false);
			SplineMeshComponent->SetEndScale(FVector2D(WidthScale, HeightScale), false);
			SplineMeshComponent->MarkRenderStateDirty();
			SplineMeshComponent->CreationMethod = EComponentCreationMethod::UserConstructionScript;
			SplineMeshComponent->RegisterComponent();
			AddInstanceComponent(SplineMeshComponent);
			SplineMeshComponents.Add(SplineMeshComponent);
			
			AsyncTask(ENamedThreads::GameThread, [=, this]() {
				OnSplineMeshCreated(SplineComponent, SplineMeshComponent);
			});
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