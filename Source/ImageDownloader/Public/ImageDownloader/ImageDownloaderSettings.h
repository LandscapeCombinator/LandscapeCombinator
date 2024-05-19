// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "ImageDownloaderSettings.generated.h"

UCLASS(config=EditorPerProjectUserSettings, meta=(DisplayName="Landscape Combinator"))
class IMAGEDOWNLOADER_API UImageDownloaderSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UImageDownloaderSettings()
	{
		CategoryName = "Plugins";
	}

	UPROPERTY(config, EditAnywhere, Category = "LandscapeCombinator")
	FString MapTiler_Token = "";

	UPROPERTY(config, EditAnywhere, Category = "LandscapeCombinator")
	FString Mapbox_Token = "";
};
