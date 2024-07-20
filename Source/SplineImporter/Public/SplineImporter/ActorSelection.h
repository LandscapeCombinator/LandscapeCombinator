// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ActorSelection.generated.h"

UENUM(BlueprintType)
enum class EActorSelectionMode: uint8 {
	Actor,
	ActorTag
};


USTRUCT(BlueprintType)
struct SPLINEIMPORTER_API FActorSelection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorSelection",
		meta = (DisplayPriority = "1")
	)
	EActorSelectionMode ActorSelectionMode = EActorSelectionMode::ActorTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorSelection",
		meta = (EditCondition = "ActorSelectionMode == EActorSelectionMode::Actor", EditConditionHides, DisplayPriority = "2")
	)
	AActor *Actor = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ActorSelection",
		meta = (EditCondition = "ActorSelectionMode == EActorSelectionMode::ActorTag", EditConditionHides, DisplayPriority = "2")
	)
	FName ActorTag;

	AActor* GetActor(UWorld *World);
};
