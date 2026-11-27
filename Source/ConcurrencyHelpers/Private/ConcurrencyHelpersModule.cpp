// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "ConcurrencyHelpersModule.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "Engine/World.h"

void FConcurrencyHelpersModule::StartupModule()
{
	WorldTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddLambda([](UWorld* World)
	{
		if (IsValid(World) && (World->IsGameWorld() || World->WorldType == EWorldType::Editor))
		{
			Concurrency::SetCancelRequested(true);
		}
	});

	WorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddLambda([](UWorld* World, const UWorld::InitializationValues)
	{
		if (IsValid(World) && (World->IsGameWorld() || World->WorldType == EWorldType::Editor))
		{
			Concurrency::SetCancelRequested(false);
		}
	});
}

void FConcurrencyHelpersModule::ShutdownModule()
{
	FWorldDelegates::OnWorldBeginTearDown.Remove(WorldTearDownHandle);
	FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitHandle);
}

IMPLEMENT_MODULE(FConcurrencyHelpersModule, ConcurrencyHelpers)
