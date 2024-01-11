// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "ImageDownloaderSettings.generated.h"

UCLASS(Config=CurrencySystem, meta=(DisplayName="Landscape Combinator"))
class IMAGEDOWNLOADER_API UImageDownloaderSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UImageDownloaderSettings()
	{
		CategoryName = "Plugins";
	}

	UPROPERTY(Config, EditAnywhere)
	FString Mapbox_Token;
};
