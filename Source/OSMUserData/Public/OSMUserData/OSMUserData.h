// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "OSMUserData.generated.h"

#define LOCTEXT_NAMESPACE "FOSMUserDataModule"

UCLASS()
class OSMUSERDATA_API UOSMUserData : public UAssetUserData
{
	GENERATED_BODY()

public:
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ExternalTool",
		meta = (EditConditionHides, DisplayPriority = "1")
	)
	float Height = 0;
};

#undef LOCTEXT_NAMESPACE
