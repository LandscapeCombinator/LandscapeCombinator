// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloaderModule.h"
#include "PropertyEditorModule.h"

#include "ImageDownloader/BasicImageDownloaderCustomization.h"
#include "ImageDownloader/BasicImageDownloader.h"
	
IMPLEMENT_MODULE(FImageDownloaderModule, ImageDownloader)

void FImageDownloaderModule::StartupModule()
{
    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    PropertyModule.RegisterCustomClassLayout(ABasicImageDownloader::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FBasicImageDownloaderCustomization::MakeInstance));
}
