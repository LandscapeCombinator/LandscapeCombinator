// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LCCommon/LCBlueprintLibrary.h"
#include "LCCommon/LogLCCommon.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SplineComponent.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h" 

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

bool ULCBlueprintLibrary::GetFirstPlayerPosition(const UWorld *World, FVector &OutPosition)
{
	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0))
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
	if (!IsValid(World)) return Actors;

	if (Tag.IsNone()) UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), Actors);
	else UGameplayStatics::GetAllActorsWithTag(World, Tag, Actors);

	return Actors;
}

TSet<TObjectPtr<USplineComponent>> ULCBlueprintLibrary::FindSplineComponents(UWorld *World, bool bIsUserInitiated, FName Tag, FName ComponentTag)
{
	check(IsInGameThread());

	TSet<TObjectPtr<USplineComponent>> Result;

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

bool ULCBlueprintLibrary::FindPushOffset(TWeakObjectPtr<AActor> Actor, UPrimitiveComponent* TestComponent, FName RequiredPusherTag, int MaxSteps, double StepSize, FVector& OutOffset, bool bShowDebug)
{
    TRACE_CPUPROFILER_EVENT_SCOPE_STR("FindPushOffset");
    check(IsInGameThread());

    if (!Actor.IsValid() || !IsValid(TestComponent)) return false;
    UWorld* World = Actor->GetWorld();
    if (!IsValid(World)) return false;

    const FVector Origin = TestComponent->GetComponentLocation();
    const FQuat Rotation = TestComponent->GetComponentQuat();

    if (TestComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
        TestComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    FComponentQueryParams QueryParams(SCENE_QUERY_STAT(FindPushOffset), Actor.Get());
    QueryParams.bTraceComplex = false;

    const FBoxSphereBounds LocalBounds = TestComponent->CalcBounds(FTransform::Identity);
	FVector ShapeExtent = LocalBounds.BoxExtent;
	ShapeExtent.Z *= 10; // make the box very tall to avoid buildings going above roads and missing collision
	FCollisionShape Shape = FCollisionShape::MakeBox(ShapeExtent);
    auto DrawShape = [&](const FVector& Loc, const FColor& Color)
    {
        if (!bShowDebug) return;
        const FVector BoxCenter = Loc + Rotation.RotateVector(LocalBounds.Origin);
        DrawDebugBox(World, BoxCenter, LocalBounds.BoxExtent, Rotation, Color, false, 5.f, 0, 10.f);
    };

    auto HasOverlap = [&](const FVector& TestLocation) -> bool
    {
        TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByChannel(Overlaps, TestLocation + Rotation.RotateVector(LocalBounds.Origin), Rotation, ECC_Visibility, Shape, QueryParams);

        if (RequiredPusherTag.IsNone())
            return Overlaps.Num() > 0;

        for (const FOverlapResult& O : Overlaps)
            if (AActor* OwnerActor = O.GetActor())
                if (OwnerActor->ActorHasTag(RequiredPusherTag))
                    return true;
        return false;
    };

    const bool bInitialOverlap = HasOverlap(Origin);
    DrawShape(Origin, bInitialOverlap ? FColor::Red : FColor::Green);
    if (!bInitialOverlap) return false;

    static const FVector2D Directions[] =
    {
        { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 },
        { 0.7071f,  0.7071f }, { 0.7071f, -0.7071f },
        { -0.7071f, 0.7071f }, { -0.7071f, -0.7071f }
    };

    for (const FVector2D& D2 : Directions)
    {
        const FVector Dir(D2.X, D2.Y, 0);
        for (int Step = 1; Step <= MaxSteps; Step++)
        {
            const FVector Candidate = Origin + Dir * (StepSize * Step);
            const bool bCandidateOverlap = HasOverlap(Candidate);
            DrawShape(Candidate, bCandidateOverlap ? FColor::Red : FColor::Green);

            if (!bCandidateOverlap)
            {
                OutOffset = Dir * (StepSize * Step);
                return true;
            }
        }
    }

    return false;
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
		FScopedTransaction Transaction(NSLOCTEXT("LandscapeCombinator", "DeleteFolder", "Delete Folder"));
		FActorFolders::Get().DeleteFolder(World, Folder);
		FFolder Parent = Folder.GetParent();
		if (Parent != Folder.GetRootObject()) DeleteFolder(World, Parent);
	}
}

#endif

#undef LOCTEXT_NAMESPACE
