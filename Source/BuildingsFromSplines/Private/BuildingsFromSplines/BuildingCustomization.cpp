// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "BuildingsFromSplines/BuildingCustomization.h"

#if WITH_EDITOR

#include "DetailLayoutBuilder.h"

TSharedRef<IDetailCustomization> FBuildingCustomization::MakeInstance()
{
	return MakeShareable(new FBuildingCustomization);
}

void FBuildingCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.EditCategory("Building", FText::GetEmpty(), ECategoryPriority::Important);
	DetailBuilder.EditCategory("BuildingConfigurationConversion", FText::GetEmpty(), ECategoryPriority::Important);
}

#endif
