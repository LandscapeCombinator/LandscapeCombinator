// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LCCommon/ActorSelection.h"
#include "LCReporter/LCReporter.h"

#include "CoreMinimal.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/MessageDialog.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"


AActor* FActorSelection::GetActor(const UWorld* World)
{
	if (!IsValid(World))
	{
		ULCReporter::ShowError(
			LOCTEXT("ActorSelection::GetActor::InvalidWorld", "InvalidWorld")
		);
		return nullptr;
	}

	switch (ActorSelectionMode)
	{
		case EActorSelectionMode::Actor:
		{
			if (!Actor.IsValid())
			{
				ULCReporter::ShowError(
					LOCTEXT("ActorSelection::GetActor", "Landscape Combinator Actor Selection: Please select a valid actor")
				);
				return nullptr;
			};
			return Actor.Get();
		}

		case EActorSelectionMode::ActorTag:
		{
			TArray<AActor*> Actors;
			UGameplayStatics::GetAllActorsOfClassWithTag(World, AActor::StaticClass(), ActorTag, Actors);

			if (Actors.IsEmpty())
			{
				ULCReporter::ShowError(FText::Format(
					LOCTEXT("ActorSelection::GetActor", "Landscape Combinator Actor Selection: Could not find actor with tag {0}"),
					FText::FromName(ActorTag)
				));
				return nullptr;
			}
			else
			{
				return Actors[0];
			}
		}

		default:
			return nullptr;
	}
}

TArray<AActor*> FActorSelection::GetAllActors(const UWorld* World)
{
	if (!IsValid(World))
	{
		ULCReporter::ShowError(
			LOCTEXT("ActorSelection::GetAllActors::InvalidWorld", "InvalidWorld")
		);
		return {};
	}

	switch (ActorSelectionMode)
	{
		case EActorSelectionMode::Actor:
		{
			if (!Actor.IsValid())
			{
				ULCReporter::ShowError(
					LOCTEXT("ActorSelection::GetAllActors", "Landscape Combinator Actor Selection: Please select a valid actor")
				);
				return {};
			};
			return { Actor.Get() };
		}

		case EActorSelectionMode::ActorTag:
		{
			TArray<AActor*> Actors;
			UGameplayStatics::GetAllActorsOfClassWithTag(World, AActor::StaticClass(), ActorTag, Actors);

			if (Actors.IsEmpty())
			{
				ULCReporter::ShowError(FText::Format(
					LOCTEXT("ActorSelection::GetAllActors", "Landscape Combinator Actor Selection: Could not find actor with tag {0}"),
					FText::FromName(ActorTag)
				));
				return {};
			}
			else
			{
				return Actors;
			}
		}

		default:
			return {};
	}
}


#undef LOCTEXT_NAMESPACE