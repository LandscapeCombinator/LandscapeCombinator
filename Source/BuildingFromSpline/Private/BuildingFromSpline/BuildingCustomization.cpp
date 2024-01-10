// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "BuildingFromSpline/BuildingCustomization.h"

#if WITH_EDITOR

#include "DetailLayoutBuilder.h"

TSharedRef<IDetailCustomization> FBuildingCustomization::MakeInstance()
{
	return MakeShareable(new FBuildingCustomization);
}

void FBuildingCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.EditCategory("Building", FText::GetEmpty(), ECategoryPriority::Important);
}

#endif
