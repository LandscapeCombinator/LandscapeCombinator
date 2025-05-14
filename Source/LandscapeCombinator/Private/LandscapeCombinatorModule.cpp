// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinatorModule.h"
#include "ConcurrencyHelpers/LCReporter.h"

#if WITH_EDITOR

#include "LandscapeCombinator/LandscapeCombinatorCommands.h"
#include "LandscapeCombinator/LandscapeSpawner.h"
#include "LandscapeCombinator/LandscapeSpawnerCustomization.h"
#include "LandscapeCombinator/LandscapeTexturer.h"
#include "LandscapeCombinator/LandscapeTexturerCustomization.h"
#include "LandscapeCombinator/LandscapeMesh.h"
#include "LandscapeCombinator/LandscapeMeshCustomization.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"


#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"

#endif

IMPLEMENT_MODULE(FLandscapeCombinatorModule, LandscapeCombinator)

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

#if WITH_EDITOR

void FLandscapeCombinatorModule::StartupModule()
{
	UE_LOG(LogLandscapeCombinator, Log, TEXT("LandscapeCombinator StartupModule"));
	UE_LOG(LogLandscapeCombinator, Warning,
		TEXT("Setting geometry.DynamicMesh.MaxComplexCollisionTriCount to 2147483647 to make sure "
			 "that collisions for dynamic meshes are correctly generated."));
	IConsoleVariable* CVar_MaxComplexCollisionTriCount = IConsoleManager::Get().FindConsoleVariable(TEXT("geometry.DynamicMesh.MaxComplexCollisionTriCount"));
	if (CVar_MaxComplexCollisionTriCount) CVar_MaxComplexCollisionTriCount->Set(2147483647);

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(ALandscapeMesh::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FLandscapeMeshCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(ALandscapeSpawner::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FLandscapeSpawnerCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(ALandscapeTexturer::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FLandscapeTexturerCustomization::MakeInstance));

	FLandscapeCombinatorStyle::Initialize();
	FLandscapeCombinatorStyle::ReloadTextures();

	FLandscapeCombinatorCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FLandscapeCombinatorCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FLandscapeCombinatorModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FLandscapeCombinatorModule::RegisterMenus));
}

void FLandscapeCombinatorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FLandscapeCombinatorStyle::Shutdown();

	FLandscapeCombinatorCommands::Unregister();
}

void FLandscapeCombinatorModule::PluginButtonClicked()
{
	LCReporter::ShowError(
		LOCTEXT(
			"WidgetDisabled",
			"This widget has been disabled for now. Please use Landscape Combination actors directly as in the L_LandscapeCombinatorExamples map."
		)
	);	

	return;
	// const FString WidgetPath = TEXT("/Script/Blutility.EditorUtilityWidgetBlueprint'/LandscapeCombinator/UI/LandscapeCombinator.LandscapeCombinator'");
	
	// UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
	// UEditorUtilityWidgetBlueprint* WidgetBlueprint = LoadObject<UEditorUtilityWidgetBlueprint>(nullptr, *WidgetPath);
	// if (EditorUtilitySubsystem && WidgetBlueprint)
	// {
	// 	EditorUtilitySubsystem->SpawnAndRegisterTab(WidgetBlueprint);
	// }
}

void FLandscapeCombinatorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
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
