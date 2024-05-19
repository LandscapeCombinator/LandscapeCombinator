// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeCombinatorBlueprintLibrary.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"


void ULandscapeCombinatorBlueprintLibrary::SortByLabel(TArray<AActor*> &Actors)
{
	Actors.Sort([](const AActor& Actor1, const AActor& Actor2) {
		return Actor1.GetActorNameOrLabel().Compare(Actor2.GetActorNameOrLabel()) < 0;
	});
}


#undef LOCTEXT_NAMESPACE