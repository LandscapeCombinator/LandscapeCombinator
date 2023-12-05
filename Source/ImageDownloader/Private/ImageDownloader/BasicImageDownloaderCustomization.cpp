// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloader/BasicImageDownloaderCustomization.h"

#include "DetailLayoutBuilder.h"

TSharedRef<IDetailCustomization> FBasicImageDownloaderCustomization::MakeInstance()
{
	return MakeShareable(new FBasicImageDownloaderCustomization);
}

void FBasicImageDownloaderCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.EditCategory("BasicImageDownloader", FText::GetEmpty(), ECategoryPriority::Important);
}
