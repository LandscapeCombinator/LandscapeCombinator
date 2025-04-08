// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Engine/AssetUserData.h"

#include "OSMUserData.generated.h"

#define LOCTEXT_NAMESPACE "FOSMUserDataModule"

UCLASS()
class OSMUSERDATA_API UOSMUserData : public UAssetUserData
{
	GENERATED_BODY()

public:
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "OSMUserData",
		meta = (EditConditionHides, DisplayPriority = "1")
	)
	TMap<FString, FString> Fields;
};

#undef LOCTEXT_NAMESPACE
