// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#if WITH_EDITOR

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "IPropertyTypeCustomization.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

class IDetailLayoutBuilder;

class FLandscapeSpawnerCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;
};

#undef LOCTEXT_NAMESPACE

#endif
