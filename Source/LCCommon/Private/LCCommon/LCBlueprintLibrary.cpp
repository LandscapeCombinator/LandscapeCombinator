// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LCCommon/LCBlueprintLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Engine.h"

#if WITH_EDITOR
#include "EditorActorFolders.h"
#endif

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

void ULCBlueprintLibrary::SortByLabel(UPARAM(ref) TArray<AActor*> &Actors)
{
	Actors.Sort([](const AActor& Actor1, const AActor& Actor2) {
		return Actor1.GetActorNameOrLabel().Compare(Actor2.GetActorNameOrLabel()) < 0;
	});
}

FString ULCBlueprintLibrary::Replace(FString Original, FString String1, FString String2)
{
	return Original.Replace(*String1, *String2);
}

FName ULCBlueprintLibrary::ReplaceName(FName Original, FName String1, FName String2)
{
	return FName(Replace(Original.ToString(), String1.ToString(), String2.ToString()));
}

template<typename T>
void ULCBlueprintLibrary::GetSortedActorsOfClassWithTag(const UWorld* World, FName Tag, TArray<T*>& OutActors)
{
	for (TActorIterator<T> It(World); It; ++It)
	{
		T* Actor = *It;
		if (IsValid(Actor) && Actor->ActorHasTag(Tag))
		{
			OutActors.Add(Actor);
		}
	}
	
	OutActors.Sort([](const T& Actor1, const T& Actor2) {
		return Actor1.GetActorNameOrLabel().Compare(Actor2.GetActorNameOrLabel()) < 0;
	});
}




#if WITH_EDITOR

void ULCBlueprintLibrary::SetFolderPath2(AActor* Actor, FName FolderPathOverride, FName FolderPath)
{
	if (!IsValid(Actor)) return;

	if (!FolderPathOverride.IsNone())
	{
		Actor->SetFolderPath(FolderPathOverride);
	}
	else if (!FolderPath.IsNone())
	{
		Actor->SetFolderPath(FolderPath);
	}
}

void ULCBlueprintLibrary::DeleteFolder(UWorld &World, FFolder Folder)
{
	FName FolderPath = Folder.GetPath();
	TSet<FName> InPaths = { FolderPath };
	TArray<AActor*> ActorsInFolder;

	FActorFolders::GetActorsFromFolders(World, InPaths, ActorsInFolder, Folder.GetRootObject());

	if (ActorsInFolder.Num() == 0)
	{
		FActorFolders::Get().DeleteFolder(World, Folder);
		FFolder Parent = Folder.GetParent();
		if (Parent != Folder.GetRootObject()) DeleteFolder(World, Parent);
	}
}

#endif

#undef LOCTEXT_NAMESPACE
