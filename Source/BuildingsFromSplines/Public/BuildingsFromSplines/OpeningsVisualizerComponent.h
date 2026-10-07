// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "BuildingsFromSplines/BuildingConfiguration.h"

#include "OpeningsVisualizerComponent.generated.h"

struct FOpeningHandle
{
	TWeakObjectPtr<ULevelDescription> Level;
	int32 OpeningIndex = INDEX_NONE;
	FVector WorldLocation = FVector::ZeroVector;
};

UCLASS()
class UOpeningsVisualizerComponent : public USceneComponent
{
	GENERATED_BODY()
};
