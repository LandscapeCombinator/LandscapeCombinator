// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeCombinatorBlueprintLibrary.h"
#include "LandscapeUtils/LandscapeUtils.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void ULandscapeCombinatorBlueprintLibrary::SortByLabel(UPARAM(ref) TArray<AActor*> &Actors)
{
	Actors.Sort([](const AActor& Actor1, const AActor& Actor2) {
		return Actor1.GetActorNameOrLabel().Compare(Actor2.GetActorNameOrLabel()) < 0;
	});
}

bool ULandscapeCombinatorBlueprintLibrary::GetZ(AActor *Actor, double x, double y, double &OutZ, bool bDrawDebugLine)
{
    return LandscapeUtils::GetZ(Actor, x, y, OutZ, bDrawDebugLine);
}

#if WITH_EDITOR

void ULandscapeCombinatorBlueprintLibrary::SetFolderPath(AActor* Actor, FName FolderPath)
{
	if (!IsValid(Actor)) return;

	Actor->SetFolderPath(FolderPath);

	// to prevent the folder name from being in renaming mode
	if (GEditor)
    {
		GEditor->SelectActor(Actor, true, true);
    }
}

#endif

#undef LOCTEXT_NAMESPACE