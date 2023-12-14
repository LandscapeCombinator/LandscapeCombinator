// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "BuildingFromSplineModule.h"

#include "BuildingFromSpline/Building.h"
#include "BuildingFromSpline/BuildingCustomization.h"

#define LOCTEXT_NAMESPACE "FBuildingFromSplineModule"
	
IMPLEMENT_MODULE(FBuildingFromSplineModule, BuildingFromSpline)

void FBuildingFromSplineModule::StartupModule()
{
    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    PropertyModule.RegisterCustomClassLayout(ABuilding::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FBuildingCustomization::MakeInstance));
}

#undef LOCTEXT_NAMESPACE
