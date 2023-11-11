// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinatorModule.h"
#include "LandscapeCombinator/LandscapeSpawner.h"
#include "LandscapeCombinator/LandscapeSpawnerCustomization.h"
#include "LandscapeCombinator/LandscapeTexturer.h"
#include "LandscapeCombinator/LandscapeTexturerCustomization.h"
	
IMPLEMENT_MODULE(FLandscapeCombinatorModule, LandscapeCombinator)

void FLandscapeCombinatorModule::StartupModule()
{
    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    PropertyModule.RegisterCustomClassLayout(ALandscapeSpawner::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FLandscapeSpawnerCustomization::MakeInstance));
    PropertyModule.RegisterCustomClassLayout(ALandscapeTexturer::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FLandscapeTexturerCustomization::MakeInstance));
}
