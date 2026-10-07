// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "BuildingsFromSplinesModule.h"

#include "BuildingsFromSplines/Building.h"
#include "BuildingsFromSplines/BuildingCustomization.h"


#define LOCTEXT_NAMESPACE "FBuildingsFromSplinesModule"
	
IMPLEMENT_MODULE(FBuildingsFromSplinesModule, BuildingsFromSplines)

#if WITH_EDITOR

#include "PropertyEditorModule.h"
#include "PropertyEditorDelegates.h"
#include "Editor.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "BuildingsFromSplines/OpeningsVisualizer.h"
#include "BuildingsFromSplines/AssetLinkCustomization.h"
#include "BuildingsFromSplines/AssetLink.h"
#include "Misc/CoreDelegates.h"

void FBuildingsFromSplinesModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(ABuilding::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FBuildingCustomization::MakeInstance));
	for (const FName& Name : AssetLink::TypeNames())
		PropertyModule.RegisterCustomPropertyTypeLayout(Name, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAssetLinkCustomization::MakeInstance), FAssetLinkCustomization::Identifier());

	if (GUnrealEd) RegisterVisualizer();
	else PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FBuildingsFromSplinesModule::RegisterVisualizer);
}

void FBuildingsFromSplinesModule::RegisterVisualizer()
{
	if (GUnrealEd) GUnrealEd->RegisterComponentVisualizer(UOpeningsVisualizerComponent::StaticClass()->GetFName(), MakeShared<FOpeningsVisualizer>());
}

void FBuildingsFromSplinesModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);

	if (GUnrealEd) GUnrealEd->UnregisterComponentVisualizer(UOpeningsVisualizerComponent::StaticClass()->GetFName());

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout(ABuilding::StaticClass()->GetFName());
		for (const FName& Name : AssetLink::TypeNames())
			PropertyModule.UnregisterCustomPropertyTypeLayout(Name, FAssetLinkCustomization::Identifier());
	}
}

#endif

#undef LOCTEXT_NAMESPACE
