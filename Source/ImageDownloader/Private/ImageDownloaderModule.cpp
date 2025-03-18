// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "ImageDownloaderModule.h"
	
IMPLEMENT_MODULE(FImageDownloaderModule, ImageDownloader)

#if WITH_EDITOR

#include "PropertyEditorModule.h"
#include "ImageDownloader/BasicImageDownloaderCustomization.h"
#include "ImageDownloader/BasicImageDownloader.h"

void FImageDownloaderModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(ABasicImageDownloader::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FBasicImageDownloaderCustomization::MakeInstance));
}

#endif
