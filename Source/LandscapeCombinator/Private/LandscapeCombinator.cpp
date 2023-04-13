// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeCombinator.h"
#include "LandscapeCombinatorStyle.h"
#include "LandscapeCombinatorCommands.h"
#include "SlateTable.h"
#include "GlobalSettings.h"
#include "Utils/Download.h"
#include "Utils/Logging.h"
#include "Utils/Console.h"
#include "Utils/Time.h"
#include "Road/RoadBuilder.h"
#include "Road/RoadTable.h"
#include "Elevation/HeightMapTable.h"

#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Interfaces/IPluginManager.h"
#include "Landscape.h"	
#include "LandscapeStreamingProxy.h"
#include "Misc/Paths.h"
#include "PropertyCustomizationHelpers.h" 

#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterActor.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionLevelHelper.h"
#include "WorldPartition/WorldPartitionActorLoaderInterface.h"
#include "WorldPartition/WorldPartitionEditorLoaderAdapter.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

const FName FLandscapeCombinatorModule::LandscapeCombinatorTabName("LandscapeCombinator");

void FLandscapeCombinatorModule::StartupModule()
{
	// GDAL
	GDALAllRegister();
	OGRRegisterAll();
	FString ShareFolder = FPaths:: ConvertRelativePathToFull(IPluginManager::Get().FindPlugin("LandscapeCombinator")->GetBaseDir()) / "ThirdParty" / "GDAL" / "share";
	FString GDALData = (ShareFolder / "gdal").Replace(TEXT("/"), TEXT("\\"));
	FString PROJData = (ShareFolder / "proj").Replace(TEXT("/"), TEXT("\\"));
	FString OSMConf = (ShareFolder / "gdal" / "osmconf.ini").Replace(TEXT("/"), TEXT("\\"));
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Setting GDAL_DATA to: %s"), *GDALData);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Setting PROJ_DATA to: %s"), *PROJData);
	CPLSetConfigOption("GDAL_DATA", TCHAR_TO_UTF8(*GDALData));
	CPLSetConfigOption("PROJ_DATA", TCHAR_TO_UTF8(*PROJData));
	CPLSetConfigOption("GDAL_PAM_ENABLED", "NO");
	CPLSetConfigOption("OSM_CONFIG_FILE", TCHAR_TO_UTF8(*OSMConf));
	const char* const ProjPaths[] = { TCHAR_TO_UTF8(*PROJData), nullptr };
	OSRSetPROJSearchPaths(ProjPaths);

	// Plugin
	FLandscapeCombinatorStyle::Initialize();
	FLandscapeCombinatorStyle::ReloadTextures();
	FLandscapeCombinatorCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FLandscapeCombinatorCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FLandscapeCombinatorModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FLandscapeCombinatorModule::RegisterMenus));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(LandscapeCombinatorTabName, FOnSpawnTab::CreateRaw(this, &FLandscapeCombinatorModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FLandscapeCombinatorTabTitle", "LandscapeCombinator"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FLandscapeCombinatorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FLandscapeCombinatorStyle::Shutdown();

	FLandscapeCombinatorCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(LandscapeCombinatorTabName);
}

FReply FLandscapeCombinatorModule::LoadLandscapes()
{	
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Attempting to load all landscapes"));
	TArray<AActor*> LandscapeStreamingProxies;
	UWorld* World = GEditor->GetEditorWorldContext().World();
    UGameplayStatics::GetAllActorsOfClass(World, ALandscapeStreamingProxy::StaticClass(), LandscapeStreamingProxies);

	const FBox RegionBox(FVector(-HALF_WORLD_MAX, HALF_WORLD_MAX, -HALF_WORLD_MAX), FVector(HALF_WORLD_MAX, HALF_WORLD_MAX, HALF_WORLD_MAX));
	UWorldPartitionEditorLoaderAdapter* EditorLoaderAdapter = World->GetWorldPartition()->CreateEditorLoaderAdapter<FLoaderAdapterShape>(World, RegionBox, TEXT("Loaded Region"));
	EditorLoaderAdapter->GetLoaderAdapter()->SetUserCreated(true);
	EditorLoaderAdapter->GetLoaderAdapter()->Load();
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Finished loading"));

	return FReply::Handled();
}

TSharedRef<SDockTab> FLandscapeCombinatorModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	Download::LoadExpectedSizeCache();

	GlobalSettings::HMTable = new HeightMapTable();
	RoadTable* RTable = new RoadTable(GlobalSettings::HMTable);

	TSharedRef<SDockTab> Tab = SNew(SDockTab).TabRole(ETabRole::NomadTab)[
		SNew(SScrollBox)
		+SScrollBox::Slot().Padding(FMargin(30, 0, 0, 0))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(FMargin(0, 30, 0, 50))
				[
					SNew(STextBlock)
					.Text(FText::FromString("Landscape Combinator"))
					.Font(FLandscapeCombinatorStyle::TitleFont())
				]
			+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0, 0, 0, 30))
				[
					SNew(STextBlock)
					.Text(FText::FromString("1. Global World Settings"))
					.Font(FLandscapeCombinatorStyle::SubtitleFont())
				]
			+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(50, 0, 0, 30))
				[
					GlobalSettings::GlobalSettingsTable()
				]
			+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0, 0, 0, 30))
				[
					SNew(STextBlock)
					.Text(FText::FromString("2. Landscape Elevation"))
					.Font(FLandscapeCombinatorStyle::SubtitleFont())
				]
			+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(50, 0, 80, 40))
				[
					GlobalSettings::HMTable->MakeTable()
				]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0, 0, 0, 30))
				[
					SNew(STextBlock)
					.Text(FText::FromString("3. Roads (or Landscape Splines)"))
					.Font(FLandscapeCombinatorStyle::SubtitleFont())
				]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(50, 0, 80, 40))
				[
					RTable->MakeTable()
				]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0, 0, 0, 30))
				[
					SNew(STextBlock)
					.Text(FText::FromString("4. Foliage"))
				.Font(FLandscapeCombinatorStyle::SubtitleFont())
				]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(50, 0, 80, 40))
				[
					SNew(SEditableText)
					.Text(LOCTEXT("LandscapeFoliageExplanation",
						"Please check the README file on https://github.com/LandscapeCombinator/LandscapeCombinator#procedural-foliage to add foliage."
					))
					.IsReadOnly(true)
					.Font(FLandscapeCombinatorStyle::RegularFont())
				]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0, 0, 0, 30))
				[
					SNew(STextBlock)
					.Text(FText::FromString("5. Buildings"))
				.Font(FLandscapeCombinatorStyle::SubtitleFont())
				]
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(50, 0, 80, 40))
				[
					SNew(SEditableText)
					.Text(LOCTEXT("Buildings teaser",
						"Soon?"
					))
					.IsReadOnly(true)
					.Font(FLandscapeCombinatorStyle::RegularFont())
				]
		]
	];
	return Tab;
}

void FLandscapeCombinatorModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(LandscapeCombinatorTabName);
}

void FLandscapeCombinatorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FLandscapeCombinatorCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FLandscapeCombinatorCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLandscapeCombinatorModule, LandscapeCombinator)
