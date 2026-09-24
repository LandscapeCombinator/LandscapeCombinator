// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinatorModule.h"
#include "ConcurrencyHelpers/LCReporter.h"
#include "LandscapeCombinator/GeneratorsStatusOverlayManager.h"

#if WITH_EDITOR

#include "LandscapeCombinator/LandscapeCombinatorCommands.h"
#include "LandscapeCombinator/LandscapeSpawner.h"
#include "LandscapeCombinator/LandscapeSpawnerCustomization.h"
#include "LandscapeCombinator/LandscapeTexturer.h"
#include "LandscapeCombinator/LandscapeTexturerCustomization.h"
#include "LandscapeCombinator/LandscapeMesh.h"
#include "LandscapeCombinator/LandscapeMeshCustomization.h"
#include "LandscapeCombinator/LogLandscapeCombinator.h"
#include "LandscapeCombinator/GeneratorWrapper.h"
#include "LandscapeCombinator/GeneratorsStatus.h"

#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "ToolMenus.h"

#include "GameDelegates.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "Blueprint/UserWidget.h"
#include "Engine/AssetManager.h"
#include "Engine/AssetManagerTypes.h"

#endif

IMPLEMENT_MODULE(FLandscapeCombinatorModule, LandscapeCombinator)

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

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

	BeginPIEHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &FLandscapeCombinatorModule::OnPIEStateChanged);
	EndPIEHandle   = FEditorDelegates::EndPIE.AddRaw(this, &FLandscapeCombinatorModule::OnPIEStateChanged);

	ModifyCookDelegateHandle = FGameDelegates::Get().GetModifyCookDelegate().AddRaw(this, &FLandscapeCombinatorModule::GetPackagesToAlwaysCook);

#endif
}

void FLandscapeCombinatorModule::ShutdownModule()
{
#if WITH_EDITOR

	FEditorDelegates::PostPIEStarted.Remove(BeginPIEHandle);
	FEditorDelegates::EndPIE.Remove(EndPIEHandle);

	FGameDelegates::Get().GetModifyCookDelegate().Remove(ModifyCookDelegateHandle);

	HideGeneratorStatusOverlay();

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FLandscapeCombinatorStyle::Shutdown();

	FLandscapeCombinatorCommands::Unregister();

#endif
}

#if WITH_EDITOR

void FLandscapeCombinatorModule::PluginButtonClicked()
{
	ToggleGeneratorStatusOverlay();
}

void FLandscapeCombinatorModule::GetPackagesToAlwaysCook(TConstArrayView<const ITargetPlatform*> TargetPlatforms, TArray<FName>& OutPackagesToCook, TArray<FName>& OutPackagesToNeverCook)
{
	OutPackagesToCook.AddUnique(TEXT("/LandscapeCombinator/UI/W_GeneratorsStatus"));
}

void FLandscapeCombinatorModule::ToggleGeneratorStatusOverlay()
{
	if (bGeneratorStatusOverlayVisible) HideGeneratorStatusOverlay();
	else ShowGeneratorStatusOverlay();
}

void FLandscapeCombinatorModule::ShowGeneratorStatusOverlay()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<SLevelViewport> ActiveViewport = LevelEditorModule.GetFirstActiveLevelViewport();
	if (!ActiveViewport.IsValid())
	{
		UE_LOG(LogLandscapeCombinator, Error, TEXT("No active level viewport to attach the generator status overlay to."));
		return;
	}

	UUserWidget* OverlayWidget = FGeneratorsStatusOverlayManager::CreateOverlayWidget(GEditor->GetEditorWorldContext().World());
	if (!OverlayWidget)
	{
		LCReporter::ShowError(LOCTEXT("MissingOverlayWidget", "Could not load the generator status overlay widget."));
		return;
	}

	ActiveViewport->AddOverlayWidget(OverlayWidget->TakeWidget());
	GeneratorStatusOverlayInstance = OverlayWidget;
	GeneratorStatusOverlayViewport = ActiveViewport;
	bGeneratorStatusOverlayVisible = true;
}

void FLandscapeCombinatorModule::HideGeneratorStatusOverlay()
{
	TSharedPtr<SLevelViewport> Viewport = GeneratorStatusOverlayViewport.Pin();
	UUserWidget* OverlayWidget = GeneratorStatusOverlayInstance.Get();

	if (Viewport.IsValid() && OverlayWidget)
	{
		Viewport->RemoveOverlayWidget(OverlayWidget->TakeWidget());
	}

	GeneratorStatusOverlayInstance.Reset();
	GeneratorStatusOverlayViewport.Reset();
	bGeneratorStatusOverlayVisible = false;
}

void FLandscapeCombinatorModule::OnPIEStateChanged(bool bIsSimulating)
{
	if (!bGeneratorStatusOverlayVisible) return;

	UGeneratorsStatus* Status = Cast<UGeneratorsStatus>(GeneratorStatusOverlayInstance.Get());
	if (!IsValid(Status)) return;

	TWeakObjectPtr<UGeneratorsStatus> WeakStatus = Status;
	GEditor->GetTimerManager()->SetTimerForNextTick([WeakStatus]()
	{
		if (IsValid(WeakStatus.Get())) WeakStatus.Get()->RefreshGeneratorsList();
	});
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
