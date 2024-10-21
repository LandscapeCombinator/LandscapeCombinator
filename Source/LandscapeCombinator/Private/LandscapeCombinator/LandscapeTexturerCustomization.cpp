// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeTexturerCustomization.h"

#if WITH_EDITOR

#include "DetailLayoutBuilder.h"

TSharedRef<IDetailCustomization> FLandscapeTexturerCustomization::MakeInstance()
{
	return MakeShareable(new FLandscapeTexturerCustomization);
}

void FLandscapeTexturerCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.EditCategory("LandscapeTexturer", FText::GetEmpty(), ECategoryPriority::Important);
}

#endif
