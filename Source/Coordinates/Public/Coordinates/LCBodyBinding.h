// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "LCBodyBinding.generated.h"

class ALevelCoordinates;

UCLASS(ClassGroup = (LandscapeCombinator), meta = (BlueprintSpawnableComponent, DisplayName = "LC Body Binding"))
class COORDINATES_API ULCBodyBinding : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "LevelCoordinates")
	TObjectPtr<ALevelCoordinates> Body = nullptr;
};
