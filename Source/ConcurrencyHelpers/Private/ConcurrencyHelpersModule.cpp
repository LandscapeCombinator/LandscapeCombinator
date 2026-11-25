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
}

void FConcurrencyHelpersModule::ShutdownModule()
{
	FWorldDelegates::OnWorldBeginTearDown.Remove(WorldTearDownHandle);
}

IMPLEMENT_MODULE(FConcurrencyHelpersModule, ConcurrencyHelpers)
