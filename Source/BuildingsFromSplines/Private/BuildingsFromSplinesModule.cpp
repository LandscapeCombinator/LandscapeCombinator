// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "BuildingsFromSplinesModule.h"

#include "BuildingsFromSplines/Building.h"
#include "BuildingsFromSplines/BuildingCustomization.h"


#define LOCTEXT_NAMESPACE "FBuildingsFromSplinesModule"
	
IMPLEMENT_MODULE(FBuildingsFromSplinesModule, BuildingsFromSplines)

#if WITH_EDITOR

#include "PropertyEditorModule.h"
#include "PropertyEditorDelegates.h"

void FBuildingsFromSplinesModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(ABuilding::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FBuildingCustomization::MakeInstance));
}

#endif

#undef LOCTEXT_NAMESPACE
