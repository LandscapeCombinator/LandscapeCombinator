// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/LandscapeMeshCustomization.h"

#if WITH_EDITOR

#include "DetailLayoutBuilder.h"

TSharedRef<IDetailCustomization> FLandscapeMeshCustomization::MakeInstance()
{
	return MakeShareable(new FLandscapeMeshCustomization);
}

void FLandscapeMeshCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.EditCategory("LandscapeMesh", FText::GetEmpty(), ECategoryPriority::Important);
}

#endif
