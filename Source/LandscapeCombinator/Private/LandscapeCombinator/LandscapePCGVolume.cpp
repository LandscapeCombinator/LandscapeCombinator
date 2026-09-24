// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapePCGVolume.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "LCCommon/LCBlueprintLibrary.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "ConcurrencyHelpers/Concurrency.h"

#include "PCGGraph.h"
#include "Helpers/PCGGraphParametersHelpers.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void ALandscapePCGVolume::SetPositionAndBounds()
{
	AActor *TargetActor = LandscapeSelection.GetActor(GetWorld(), false);
	if (!IsValid(TargetActor)) return; // silently return when tag is invalid

	FVector2D MinMaxX, MinMaxY, MinMaxZ;
	if (!LandscapeUtils::GetActorBounds(TargetActor, MinMaxX, MinMaxY, MinMaxZ))
	{
		UE_LOG(LogLandscapeCombinator, Error, TEXT("Could not compute bounds of actor %s."), *TargetActor->GetActorNameOrLabel());
		return;
	}

	Position = FVector((MinMaxX[0] + MinMaxX[1]) / 2, (MinMaxY[0] + MinMaxY[1]) / 2, 0);
	Bounds = FVector((MinMaxX[1] - MinMaxX[0]) / 2, (MinMaxY[1] - MinMaxY[0]) / 2, 10000000);
	SetActorLocation(Position);
	SetActorScale3D(Bounds / 100);
}

void ALandscapePCGVolume::SetHugeBounds()
{
	SetActorLocation(FVector::ZeroVector);
	SetActorScale3D(FVector(HALF_WORLD_MAX / 200.0)); // half-extent ~ HALF_WORLD_MAX / 2, centered on the origin
}

bool ALandscapePCGVolume::OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated)
{
	Modify();

	if (!IsValid(PCGComponent)) return false;

	Concurrency::RunOnGameThreadAndWait([this]() {
		SetPositionAndBounds();
		return true;
	});
	PCGComponent->Generate(true);
	return true;
}

bool ALandscapePCGVolume::Cleanup_Implementation(bool bSkipPrompt)
{
	Modify();
	
	if (!IsValid(PCGComponent)) return false;

	PCGComponent->Cleanup();
	return true;
}

#if WITH_EDITOR

AActor *ALandscapePCGVolume::Duplicate(FName FromName, FName ToName)
{
	if (ALandscapePCGVolume *NewLandscapePCGVolume =
		Cast<ALandscapePCGVolume>(GEditor->GetEditorSubsystem<UEditorActorSubsystem>()->DuplicateActor(this)))
	{
		NewLandscapePCGVolume->LandscapeSelection.ActorTag =
			ULCBlueprintLibrary::ReplaceName(LandscapeSelection.ActorTag, FromName, ToName);

		UPCGGraphInstance *NewGraphInstance = NewLandscapePCGVolume->PCGComponent->GetGraphInstance();
		FPCGOverrideInstancedPropertyBag &NewParametersOverrides = NewGraphInstance->ParametersOverrides;
		for (TFieldIterator<FProperty> It(NewParametersOverrides.Parameters.GetPropertyBagStruct()); It; ++It)
		{
			FProperty* Property = *It;
			FName OldPropertyValue = UPCGGraphParametersHelpers::GetNameParameter(NewGraphInstance, Property->GetFName());
			UPCGGraphParametersHelpers::SetNameParameter(NewGraphInstance, Property->GetFName(), ULCBlueprintLibrary::ReplaceName(OldPropertyValue, FromName, ToName));
		}

		return NewLandscapePCGVolume;
	}
	else
	{
		LCReporter::ShowError(LOCTEXT("ALandscapePCGVolume::DuplicateActor", "Failed to duplicate actor."));
		return nullptr;
	}
}

#endif

#undef LOCTEXT_NAMESPACE
