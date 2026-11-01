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
#include "Misc/EngineVersionComparison.h"

void FBuildingsFromSplinesModule::StartupModule()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(ABuilding::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FBuildingCustomization::MakeInstance));
	for (const FName& Name : AssetLink::TypeNames())
		PropertyModule.RegisterCustomPropertyTypeLayout(Name, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAssetLinkCustomization::MakeInstance), FAssetLinkCustomization::Identifier());

	if (GUnrealEd) RegisterVisualizer();
	else
	{
#if UE_VERSION_OLDER_THAN(5, 8, 0)
		PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FBuildingsFromSplinesModule::RegisterVisualizer);
#else
		PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FBuildingsFromSplinesModule::RegisterVisualizer);
#endif
	}
}

void FBuildingsFromSplinesModule::RegisterVisualizer()
{
	if (GUnrealEd) GUnrealEd->RegisterComponentVisualizer(UOpeningsVisualizerComponent::StaticClass()->GetFName(), MakeShared<FOpeningsVisualizer>());
}

void FBuildingsFromSplinesModule::ShutdownModule()
{
#if UE_VERSION_OLDER_THAN(5, 8, 0)
	FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
#else
	FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
#endif

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
