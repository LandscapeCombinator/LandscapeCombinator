// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeSpawnerCustomization.h"

#include "DetailLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"


#undef LOCTEXT_NAMESPACE

TSharedRef<IDetailCustomization> FLandscapeSpawnerCustomization::MakeInstance()
{
	return MakeShareable(new FLandscapeSpawnerCustomization);
}

void FLandscapeSpawnerCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.EditCategory("LandscapeSpawner", FText::GetEmpty(), ECategoryPriority::Important);
}
