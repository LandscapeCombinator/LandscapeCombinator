// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "LandscapeCombinatorBlueprintLibrary.generated.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

UCLASS()
class ULandscapeCombinatorBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

    UFUNCTION(BlueprintCallable, Category="LandscapeCombinator")
    static void SortByLabel(UPARAM(ref) TArray<AActor*>& Actors);

    UFUNCTION(BlueprintCallable, Category="LandscapeCombinator")
	static bool GetZ(AActor* Actor, double x, double y, double &OutZ, bool bDrawDebugLine = false);

#if WITH_EDITOR

    UFUNCTION(BlueprintCallable, Category="LandscapeCombinator")
    static void SetFolderPath(AActor *Actor, FName FolderPath);

#endif
};


#undef LOCTEXT_NAMESPACE