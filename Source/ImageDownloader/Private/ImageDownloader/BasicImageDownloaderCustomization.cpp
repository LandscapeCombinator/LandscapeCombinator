// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/BasicImageDownloaderCustomization.h"

#if WITH_EDITOR

#include "DetailLayoutBuilder.h"

TSharedRef<IDetailCustomization> FBasicImageDownloaderCustomization::MakeInstance()
{
	return MakeShareable(new FBasicImageDownloaderCustomization);
}

void FBasicImageDownloaderCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.EditCategory("BasicImageDownloader", FText::GetEmpty(), ECategoryPriority::Important);
}

#endif