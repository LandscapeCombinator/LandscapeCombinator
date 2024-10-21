// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinatorModule.h"

#if WITH_EDITOR

#include "LandscapeCombinator/LandscapeCombinatorCommands.h"
#include "LandscapeCombinator/LandscapeSpawner.h"
#include "LandscapeCombinator/LandscapeSpawnerCustomization.h"
#include "LandscapeCombinator/LandscapeTexturer.h"
#include "LandscapeCombinator/LandscapeTexturerCustomization.h"

#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "EditorUtilitySubsystem.h"
#include "EditorUtilityWidgetBlueprint.h"

#endif
	
IMPLEMENT_MODULE(FLandscapeCombinatorModule, LandscapeCombinator)

#if WITH_EDITOR

void FLandscapeCombinatorModule::StartupModule()
{
    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
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
    const FString WidgetPath = TEXT("/Script/Blutility.EditorUtilityWidgetBlueprint'/LandscapeCombinator/UI/LandscapeCombinator.LandscapeCombinator'");
	
	UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();
	UEditorUtilityWidgetBlueprint* WidgetBlueprint = LoadObject<UEditorUtilityWidgetBlueprint>(nullptr, *WidgetPath);
	if (EditorUtilitySubsystem && WidgetBlueprint)
	{
		EditorUtilitySubsystem->SpawnAndRegisterTab(WidgetBlueprint);
	}
}

void FLandscapeCombinatorModule::RegisterMenus()
{
	UE_LOG(LogTemp, Error, TEXT("RegisterMenus"));

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
