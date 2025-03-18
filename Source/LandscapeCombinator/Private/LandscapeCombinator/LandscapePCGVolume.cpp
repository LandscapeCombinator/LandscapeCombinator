// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapePCGVolume.h"
#include "LCReporter/LCReporter.h"
#include "LCCommon/LCBlueprintLibrary.h"

#include "PCGGraph.h"
#include "Helpers/PCGGraphParametersHelpers.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void ALandscapePCGVolume::SetPositionAndBounds()
{
	ALandscape *Landscape = Cast<ALandscape>(LandscapeSelection.GetActor(GetWorld()));
	if (!IsValid(Landscape)) return;

	FVector2D MinMaxX, MinMaxY, MinMaxZ;
	if (!LandscapeUtils::GetLandscapeBounds(Landscape, MinMaxX, MinMaxY, MinMaxZ)) return;

	Position = FVector((MinMaxX[0] + MinMaxX[1]) / 2, (MinMaxY[0] + MinMaxY[1]) / 2, 0);
	Bounds = FVector((MinMaxX[1] - MinMaxX[0]) / 2, (MinMaxY[1] - MinMaxY[0]) / 2, 10000000);
	SetActorLocation(Position);
	SetActorScale3D(Bounds / 100);
}

void ALandscapePCGVolume::OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated, TFunction<void(bool)> OnComplete)
{
	if (IsValid(PCGComponent))
	{
		PCGComponent->Generate(true);
		OnComplete(true);
	}
	else
	{
		OnComplete(false);
	}
}

bool ALandscapePCGVolume::Cleanup_Implementation(bool bSkipPrompt)
{
	if (IsValid(PCGComponent))
	{
		PCGComponent->Cleanup();
	}
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
		ULCReporter::ShowError(LOCTEXT("ALandscapePCGVolume::DuplicateActor", "Failed to duplicate actor."));
		return nullptr;
	}
}

#endif

#undef LOCTEXT_NAMESPACE
