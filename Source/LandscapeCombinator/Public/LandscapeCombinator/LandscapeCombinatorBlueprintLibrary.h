// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "LandscapeCombinatorBlueprintLibrary.generated.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

UCLASS()
class ULandscapeCombinatorBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

    UFUNCTION(BlueprintCallable, Category="LandscapeCombinator")
    static void SortByLabel(UPARAM(ref) TArray<AActor*>& Actors);
};


#undef LOCTEXT_NAMESPACE