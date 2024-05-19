// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "SplineImporter/ActorSelection.h"

#include "Kismet/GameplayStatics.h"


#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"


AActor* FActorSelection::GetActor(UWorld* World)
{
	switch (ActorSelectionMode)
	{
		case EActorSelectionMode::Actor:
		{
			if (!IsValid(Actor))
			{
				FMessageDialog::Open(EAppMsgType::Ok,
					LOCTEXT("ActorSelection::GetActor", "Landscape Combinator Actor Selection: Please select a valid actor")
				);
				return nullptr;
			};
			return Actor;
		}

		case EActorSelectionMode::ActorTag:
		{
			TArray<AActor*> Actors;
			UGameplayStatics::GetAllActorsOfClassWithTag(World, AActor::StaticClass(), ActorTag, Actors);

			if (Actors.IsEmpty())
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
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

#undef LOCTEXT_NAMESPACE