// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "LCSettings.generated.h"

UCLASS(config=EditorPerProjectUserSettings, meta=(DisplayName="Landscape Combinator"))
class LCCOMMON_API ULCSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	ULCSettings()
	{
		CategoryName = "Plugins";
	}

	UPROPERTY(config, EditAnywhere, Category = "LandscapeCombinator", meta=(DisplayPriority = "0"))
	/* Folder to hold the downloaded and processed files */
	FString TemporaryFolder = "";

	UPROPERTY(config, EditAnywhere, Category = "LandscapeCombinator", meta=(DisplayPriority = "100", DisplayName="MapTiler Token"))
	FString MapTiler_Token = "";

	UPROPERTY(config, EditAnywhere, Category = "LandscapeCombinator", meta=(DisplayPriority = "101"))
	FString Mapbox_Token = "";

	UPROPERTY(config, EditAnywhere, Category = "LandscapeCombinator", meta=(DisplayPriority = "102", DisplayName="NextZen Token"))
	FString NextZen_Token = "";
};
