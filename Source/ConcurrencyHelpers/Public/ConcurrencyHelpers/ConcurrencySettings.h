// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "ConcurrencySettings.generated.h"

UCLASS(config=EditorPerProjectUserSettings, meta=(DisplayName="Landscape Combinator"))
class CONCURRENCYHELPERS_API UConcurrencySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UConcurrencySettings()
	{
		CategoryName = "Plugins";
	}

	
	UPROPERTY(config, EditAnywhere, Category = "LandscapeCombinator", meta=(DisplayPriority = "1000", DisplayName="Game Thread Work Budget Per Tick (Seconds)"))
	float GameThreadWorkBudgetPerTickSeconds = 0.02f;
};
