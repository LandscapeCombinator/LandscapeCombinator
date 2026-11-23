// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinatorModule.h"
#include "LandscapeCombinator/GeneratorsStatusOverlayManager.h"
#include "LandscapeCombinator/LCIcons.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR

#include "LandscapeCombinator/LandscapeCombinatorCommands.h"
#include "LandscapeCombinator/LandscapeSpawner.h"
#include "LandscapeCombinator/LandscapeSpawnerCustomization.h"
#include "LandscapeCombinator/LandscapeTexturer.h"
#include "LandscapeCombinator/LandscapeTexturerCustomization.h"
#include "LandscapeCombinator/LandscapeMesh.h"
#include "LandscapeCombinator/LandscapeMeshCustomization.h"
#include "LandscapeCombinator/AdvancedSubCategoriesCustomization.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "LandscapeCombinator/GeneratorWrapper.h"
#include "LandscapeCombinator/SGeneratorsDock.h"

#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "ToolMenus.h"

#include "GameDelegates.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

#endif

IMPLEMENT_MODULE(FLandscapeCombinatorModule, LandscapeCombinator)

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

#if WITH_EDITOR
namespace
{
	const FName GeneratorsTabName(TEXT("LandscapeCombinatorGenerators"));
}
#endif

void FLandscapeCombinatorModule::StartupModule()
{
	IConsoleVariable* CVar_MaxComplexCollisionTriCount = IConsoleManager::Get().FindConsoleVariable(TEXT("geometry.DynamicMesh.MaxComplexCollisionTriCount"));
	if (CVar_MaxComplexCollisionTriCount) CVar_MaxComplexCollisionTriCount->Set(2147483647);

#if WITH_EDITOR

	UE_LOG(LogLandscapeCombinator, Log, TEXT("LandscapeCombinator StartupModule"));

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(ALandscapeMesh::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FLandscapeMeshCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(ALandscapeSpawner::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FLandscapeSpawnerCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(ALandscapeTexturer::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FLandscapeTexturerCustomization::MakeInstance));
	for (const TCHAR* Name : { TEXT("RoadsFromSplines"), TEXT("OGRGeometry"), TEXT("BuildingConfiguration") }) PropertyModule.RegisterCustomClassLayout(Name, FOnGetDetailCustomizationInstance::CreateStatic(&FAdvancedSubCategoriesCustomization::MakeInstance));

	PropertyModule.RegisterCustomPropertyTypeLayout("GeneratorWrapper", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGeneratorWrapperCustomization::MakeInstance));

	FLandscapeCombinatorStyle::Initialize();
	FLandscapeCombinatorStyle::ReloadTextures();

	FLandscapeCombinatorCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FLandscapeCombinatorCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FLandscapeCombinatorModule::PluginButtonClicked),
		FCanExecuteAction());


	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FLandscapeCombinatorModule::RegisterMenus));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(GeneratorsTabName, FOnSpawnTab::CreateRaw(this, &FLandscapeCombinatorModule::SpawnGeneratorsTab))
		.SetDisplayName(LOCTEXT("GeneratorsTabTitle", "Landscape Combinator Generators"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	ModifyCookDelegateHandle = FGameDelegates::Get().GetModifyCookDelegate().AddRaw(this, &FLandscapeCombinatorModule::GetPackagesToAlwaysCook);

#endif
}

void FLandscapeCombinatorModule::ShutdownModule()
{
#if WITH_EDITOR

	FGameDelegates::Get().GetModifyCookDelegate().Remove(ModifyCookDelegateHandle);

	HideGeneratorStatusOverlay();

	if (FSlateApplication::IsInitialized()) FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GeneratorsTabName);

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FLandscapeCombinatorStyle::Shutdown();

	FLandscapeCombinatorCommands::Unregister();

#endif

	LCIcons::Shutdown();
}

#if WITH_EDITOR

void FLandscapeCombinatorModule::PluginButtonClicked()
{
	TSharedPtr<SDockTab> Tab = FGlobalTabmanager::Get()->FindExistingLiveTab(GeneratorsTabName);
	if (Tab.IsValid())
	{
		Tab->RequestCloseTab();
		return;
	}
	if (GeneratorStatusOverlay.IsValid())
	{
		HideGeneratorStatusOverlay();
		OpenGeneratorsTab();
		return;
	}
	ShowGeneratorStatusOverlay();
}

void FLandscapeCombinatorModule::GetPackagesToAlwaysCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& OutPackagesToCook, TArray<FName>& OutPackagesToNeverCook)
{
	OutPackagesToCook.AddUnique(TEXT("/LandscapeCombinator/Materials/M_GeoDecal"));
	OutPackagesToCook.AddUnique(TEXT("/LandscapeCombinator/Materials/M_GeoSphere"));
}

void FLandscapeCombinatorModule::ShowGeneratorStatusOverlay()
{
	if (GeneratorStatusOverlay.IsValid()) return;
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<SLevelViewport> ActiveViewport = LevelEditorModule.GetFirstActiveLevelViewport();
	if (!ActiveViewport.IsValid())
	{
		UE_LOG(LogLandscapeCombinator, Error, TEXT("No active level viewport to attach the generator status overlay to."));
		return;
	}

	TSharedRef<SWidget> Overlay = FGeneratorsStatusOverlayManager::CreateOverlayWidget(GEditor->GetEditorWorldContext().World());

	ActiveViewport->AddOverlayWidget(Overlay);
	GeneratorStatusOverlay = Overlay;
	GeneratorStatusOverlayViewport = ActiveViewport;
}

void FLandscapeCombinatorModule::HideGeneratorStatusOverlay()
{
	TSharedPtr<SLevelViewport> Viewport = GeneratorStatusOverlayViewport.Pin();
	if (Viewport.IsValid() && GeneratorStatusOverlay.IsValid())
	{
		Viewport->RemoveOverlayWidget(GeneratorStatusOverlay.ToSharedRef());
	}

	GeneratorStatusOverlay.Reset();
	GeneratorStatusOverlayViewport.Reset();
}

void FLandscapeCombinatorModule::OpenGeneratorsTab()
{
	FGlobalTabmanager::Get()->TryInvokeTab(GeneratorsTabName);
}

TSharedRef<SDockTab> FLandscapeCombinatorModule::SpawnGeneratorsTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
	.TabRole(ETabRole::NomadTab)
	[
		SNew(SGeneratorsDock)
	];
}

void FLandscapeCombinatorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
	FToolMenuSection& WindowLayoutSection = Menu->FindOrAddSection("WindowLayout");
	WindowLayoutSection.AddMenuEntryWithCommandList(FLandscapeCombinatorCommands::Get().PluginAction, PluginCommands);

	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	FToolMenuSection& PluginToolsSection = ToolbarMenu->FindOrAddSection("PluginTools");
	FToolMenuEntry& Entry = PluginToolsSection.AddEntry(FToolMenuEntry::InitToolBarButton(FLandscapeCombinatorCommands::Get().PluginAction));
	Entry.SetCommandList(PluginCommands);
}

#endif

#undef LOCTEXT_NAMESPACE
