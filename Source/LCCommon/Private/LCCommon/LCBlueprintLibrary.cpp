// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LCCommon/LCBlueprintLibrary.h"
#include "LCCommon/LogLCCommon.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SplineComponent.h"

#if WITH_EDITOR
#include "EditorViewportClient.h"
#include "EditorActorFolders.h"
#include "Editor/EditorEngine.h"
#include "Editor.h"
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

bool ULCBlueprintLibrary::GetCmPerPixelForCRS(FString CRS, int &CmPerPixel)
{
	if (CRS == "EPSG:4326" || CRS == "IGNF:WGS84G" || CRS == "EPSG:4269" || CRS == "EPSG:497" || CRS == "CRS:84")
	{
		CmPerPixel = 11111111;
		return true;
	}
	else if (CRS == "IGNF:LAMB93" || CRS == "EPSG:2154" || CRS == "EPSG:4559" || CRS == "EPSG:2056" || CRS == "EPSG:3857" || CRS == "EPSG:25832" || CRS == "EPSG:2975" || CRS == "EPSG:32633")
	{
		CmPerPixel = 100;
		return true;
	}
	else
	{
		return false;
	}
}

bool ULCBlueprintLibrary::GetEditorViewClientPosition(FVector &OutPosition)
{
#if WITH_EDITOR
	if (!GEditor) return false;

	for (FEditorViewportClient* ViewClient : GEditor->GetAllViewportClients())
	{
		if (ViewClient && ViewClient->IsPerspective())
		{
			OutPosition = ViewClient->GetViewLocation();
			return true;
		}
	}
	return false;
#else
	return false;
#endif
}

bool ULCBlueprintLibrary::GetFirstPlayerPosition(FVector &OutPosition)
{
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(GWorld, 0))
	{
		if (APawn* Pawn = PlayerController->GetPawn())
		{
			OutPosition = Pawn->GetActorLocation();
			return true;
		}
	}
	return false;
}


TArray<AActor*> ULCBlueprintLibrary::FindActors(UWorld *World, FName Tag)
{
	check(IsInGameThread());

	TArray<AActor*> Actors;

	if (Tag.IsNone()) UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
	else UGameplayStatics::GetAllActorsWithTag(World, Tag, Actors);

	return Actors;
}

TArray<USplineComponent*> ULCBlueprintLibrary::FindSplineComponents(UWorld *World, bool bIsUserInitiated, FName Tag, FName ComponentTag)
{
	check(IsInGameThread());

	TArray<USplineComponent*> Result;

	bool bFound = false;
	
	for (auto& Actor : FindActors(World, Tag))
	{
		if (ComponentTag.IsNone())
		{
			TArray<USplineComponent*> SplineComponents;
			Actor->GetComponents<USplineComponent>(SplineComponents, true);

			for (auto &SplineComponent : SplineComponents)
				Result.Add(Cast<USplineComponent>(SplineComponent));
		}
		else
		{
			for (auto &SplineComponent : Actor->GetComponentsByTag(USplineComponent::StaticClass(), ComponentTag))
				Result.Add(Cast<USplineComponent>(SplineComponent));
		}
	}

	if (bIsUserInitiated && Result.Num() == 0)
	{
		LCReporter::ShowError(
			LOCTEXT("NoSplineComponentTagged", "Could not find spline components with the given tags.")
		);
	}
	
	return Result;
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

bool ULCBlueprintLibrary::HasActor(UWorld &World, FFolder InFolder)
{
	FFolder RootObject = InFolder.GetRootObject();
	for (FActorIterator ActorIt(&World); ActorIt; ++ActorIt)
	{
		AActor* CurrentActor = *ActorIt;
		if (!IsValid(CurrentActor)) continue;
		FFolder Folder = CurrentActor->GetFolder();

		while (Folder != RootObject)
		{
			if (Folder == InFolder) return true;
			Folder = Folder.GetParent();
		}
	}

	return false;
}

void ULCBlueprintLibrary::DeleteFolder(UWorld &World, FFolder Folder)
{
	if (Folder != Folder.GetRootObject() && !HasActor(World, Folder))
	{
		FActorFolders::Get().DeleteFolder(World, Folder);
		FFolder Parent = Folder.GetParent();
		if (Parent != Folder.GetRootObject()) DeleteFolder(World, Parent);
	}
}

#endif

#undef LOCTEXT_NAMESPACE
