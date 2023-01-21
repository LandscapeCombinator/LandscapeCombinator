// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeCombinator.h"
#include "LandscapeCombinatorStyle.h"
#include "LandscapeCombinatorCommands.h"
#include "LevelEditor.h"

#include "Logging.h"
#include "Console.h"
#include "HMViewFinder1.h"
#include "HMViewFinder3.h"
#include "HMViewFinder15.h"
#include "HMRGEAlti.h"
#include "HMElevationAPI.h"
#include "HMLocalFile.h"
#include "HMURL.h"

#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Interfaces/IPluginManager.h"
#include "Landscape.h"	
#include "LandscapeStreamingProxy.h"
#include "Misc/Paths.h"

#include "WorldPartition/LoaderAdapter/LoaderAdapterShape.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterActor.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionLevelHelper.h"
#include "WorldPartition/WorldPartitionActorLoaderInterface.h"
#include "WorldPartition/WorldPartitionEditorLoaderAdapter.h"

#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

static const FName LandscapeCombinatorTabName("LandscapeCombinator");
FText KindChoice = LOCTEXT("HMInterface", "Please choose a heightmap provider");
FText EmptyDescr = LOCTEXT("HMDownloadDetails", "Coordinates for heightmap download");

FText ViewFinder15Text = LOCTEXT("ViewFinder15", "Viewfinder Panoramas 15");
FText ViewFinder3Text  = LOCTEXT("ViewFinder3", "Viewfinder Panoramas 3");
FText ViewFinder1Text  = LOCTEXT("ViewFinder1", "Viewfinder Panoramas 1");
FText RGEAltiText      = LOCTEXT("RGEAlti", "RGE Alti France");
FText ElevationAPIText = LOCTEXT("ElevationAPI", "Elevation API IGN 1m");
FText LocalFileText	   = LOCTEXT("LocalFileKind", "Local File");
FText URLText          = LOCTEXT("URLKind", "Remote URL");

TArray<TSharedPtr<FText>> Kinds = {
	MakeShareable(&ViewFinder15Text),
	MakeShareable(&ViewFinder3Text),
	MakeShareable(&ViewFinder1Text),
	MakeShareable(&RGEAltiText),
	MakeShareable(&ElevationAPIText),
	MakeShareable(&LocalFileText),
	MakeShareable(&URLText)
};

HMInterface* InterfaceFromKind(FString LandscapeName0, const FText& KindText0, FString Descr0, int Precision0)
{
	if (KindText0.EqualTo(ViewFinder15Text)) return new HMViewFinder15(LandscapeName0, KindText0, Descr0, Precision0);
	else if (KindText0.EqualTo(ViewFinder3Text)) return new HMViewFinder3(LandscapeName0, KindText0, Descr0, Precision0);
	else if (KindText0.EqualTo(ViewFinder1Text)) return new HMViewFinder1(LandscapeName0, KindText0, Descr0, Precision0);
	else if (KindText0.EqualTo(RGEAltiText)) return new HMRGEAlti(LandscapeName0, KindText0, Descr0, Precision0);
	else if (KindText0.EqualTo(ElevationAPIText)) return new HMElevationAPI(LandscapeName0, KindText0, Descr0, Precision0);
	else if (KindText0.EqualTo(LocalFileText)) return new HMLocalFile(LandscapeName0, KindText0, Descr0, Precision0);
	else if (KindText0.EqualTo(URLText)) return new HMURL(LandscapeName0, KindText0, Descr0, Precision0);
	else {
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("InterfaceFromKindError", "Internal error: heightmap kind '{0}' is not supprted."),
				KindText0
			)
		); 
		return nullptr;
	}
}

FText DescriptionFromKind(const FText& KindText0)
{
	if (KindText0.EqualTo(ViewFinder15Text)) return LOCTEXT("ViewFinder15Descr", "Enter the comma-separated list of rectangles (e.g. 15-A, 15-B, 15-G, 15-H) from http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org15.htm");
	else if (KindText0.EqualTo(ViewFinder3Text)) return LOCTEXT("ViewFinder3Descr", "Enter the comma-separated list of rectangles (e.g. M31, M32) from http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org3.htm");
	else if (KindText0.EqualTo(ViewFinder1Text)) return LOCTEXT("ViewFinder1Descr", "Enter the comma-separated list of rectangles (e.g. M31, M32) from http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org1.htm");
	else if (KindText0.EqualTo(RGEAltiText)) return LOCTEXT("RGEAltiDescr", "Enter MinLong,MaxLong,MinLat,MaxLat in EPSG 2154 coordinates");
	else if (KindText0.EqualTo(ElevationAPIText)) return LOCTEXT("ElevationAPIDescr", "Enter MinLong,MaxLong,MinLat,MaxLat in EPSG 4326 coordinates");
	else if (KindText0.EqualTo(LocalFileText)) return LOCTEXT("LocalFileDescr", "Enter your C:\\Path\\To\\MyHeightmap.tif in GeoTIFF format");
	else if (KindText0.EqualTo(URLText)) return LOCTEXT("URLDescr", "Enter URL to a heightmap in GeoTIFF format");
	else return FText();
}

void FLandscapeCombinatorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	GDALAllRegister();
	OGRRegisterAll();
	
	FString ShareFolder = FPaths:: ConvertRelativePathToFull(IPluginManager::Get().FindPlugin("LandscapeCombinator")->GetBaseDir()) / "ThirdParty" / "share";
	FString GDALData = (ShareFolder / "gdal").Replace(TEXT("/"), TEXT("\\"));
	FString PROJData = (ShareFolder / "proj").Replace(TEXT("/"), TEXT("\\"));
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Setting GDAL_DATA to: %s"), *GDALData);
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Setting PROJ_DATA to: %s"), *PROJData);
	CPLSetConfigOption("GDAL_DATA", TCHAR_TO_UTF8(*GDALData));
	CPLSetConfigOption("PROJ_DATA", TCHAR_TO_UTF8(*PROJData));
	const char* const ProjPaths[] = { TCHAR_TO_UTF8(*PROJData), nullptr };
	OSRSetPROJSearchPaths(ProjPaths);
	
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
	
	HeightMapListPluginFile  = FPaths::Combine(IPluginManager::Get().FindPlugin("LandscapeCombinator")->GetBaseDir(), "HeightmapList");
	HeightMapListProjectFile = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()) / "HeightMapList";
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
	UE_LOG(LogLandscapeCombinator, Display, TEXT("Finished loading"));

	return FReply::Handled();
}

int32 GetIndex(TSharedPtr<SVerticalBox> HeightMaps, TSharedRef<SHorizontalBox> Line) {
	for (int i = 0; i < HeightMaps->NumSlots(); i++) {
		if (HeightMaps->GetSlot(i).GetWidget() == Line) {
			return i;
		}
	}
	return 0;
}


FSlateFontInfo RegularFont = FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 12);
FSlateFontInfo TitleFont   = FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 28);
FSlateFontInfo BoldFont	   = FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 12);

void FLandscapeCombinatorModule::AddHeightMapLine(FString LandscapeName, const FText& KindText, FString Descr, int Precision, bool Save)
{
	HMInterface *Interface = InterfaceFromKind(LandscapeName, KindText, Descr, Precision);

	if (!Interface || !Interface->Initialize()) {
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("InitializationError", "There was an error while initializing {0}. Please refer to other error messages."),
				FText::FromString(LandscapeName))
		); 
		return;
	}

	TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox);
	TSharedRef<SEditableText> LandscapeNameBlock = SNew(SEditableText).Text(FText::FromString(LandscapeName)).IsReadOnly(true).Font(RegularFont);
	TSharedRef<SEditableText> KindTextBlock = SNew(SEditableText).Text(KindText).IsReadOnly(true).Font(RegularFont);
	TSharedRef<SEditableText> DescrBlock = SNew(SEditableText).Text(FText::FromString(Descr)).IsReadOnly(true).Font(RegularFont);
	TSharedRef<SEditableText> PrecisionBlock = SNew(SEditableText).Text(FText::FromString(FString::Format(TEXT("{0}%"), { Precision }))).IsReadOnly(true).Font(RegularFont);

	TSharedRef<SButton> DownloadButton = SNew(SButton)
		.OnClicked_Lambda([Interface]()->FReply {
			return Interface->DownloadHeightMaps();
		}) [
			SNew(SImage)
			.Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.Download"))
			.ToolTipText(LOCTEXT("Download", "Download Heightmaps"))
		];

	if (KindText.EqualTo(LocalFileText)) DownloadButton->SetEnabled(false);

	TSharedRef<SButton> ScaleDownButton = SNew(SButton)
		.OnClicked_Lambda([Interface]()->FReply {
			return Interface->ConvertHeightMaps();
		}) [
			SNew(SImage)
			.Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.Convert"))
			.ToolTipText(LOCTEXT("Convert", "Convert Heightmaps to PNG"))
		];

	auto AdjustLambda = [this, Interface](bool AdjustPosition){
				
		int WorldWidthKm = FCString::Atoi(*WorldWidthBlock->GetText().ToString());
		int WorldHeightKm = FCString::Atoi(*WorldHeightBlock->GetText().ToString());
		double ZScale = FCString::Atod(*ZScaleBlock->GetText().ToString());

		if (WorldWidthKm <= 0) {
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("PositiveWidth", "World Width must be greater than 0 km"));
			return FReply::Unhandled();
		}

		if (WorldHeightKm <= 0) {
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("PositiveHeight", "World Height must be greater than 0 km"));
			return FReply::Unhandled();
		}

		return Interface->AdjustLandscape(WorldWidthKm, WorldHeightKm, ZScale, true);
	};

	TSharedRef<SButton> AdjustScaleOnlyButton = SNew(SButton)
		.OnClicked_Lambda([this, Interface, AdjustLambda]()->FReply {
			return AdjustLambda(false);
		}) [
			SNew(SImage).Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.Scale"))
			.ToolTipText(LOCTEXT("AdjustScaleOnly", "Adjust scale only"))
		];

	TSharedRef<SButton> AdjustButton = SNew(SButton)
		.OnClicked_Lambda([this, Interface, AdjustLambda]()->FReply {
			return AdjustLambda(true);
		}) [
			SNew(SImage).Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.Adjust"))
			.ToolTipText(LOCTEXT("Adjust", "Adjust scale and set landscape position relative to the world"))
		];

	TSharedRef<SButton> RemoveButton = SNew(SButton)
		.OnClicked_Lambda([this, Line]()->FReply {
			HeightMaps->RemoveSlot(Line);
			SaveHeightMaps();
			return FReply::Handled();
		}) [
			SNew(SImage).Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.Delete"))
			.ToolTipText(LOCTEXT("RemoveLine", "Remove Heightmap Line"))
		];
		
	TSharedRef<SButton> UpButton = SNew(SButton)
		.OnClicked_Lambda([this, Line]()->FReply {
			int32 i = GetIndex(HeightMaps, Line);
			HeightMaps->RemoveSlot(Line);
			HeightMaps->InsertSlot(FMath::Max(0, i - 1)).AutoHeight().Padding(FMargin(0, 0, 0, 8))[ Line ];
			SaveHeightMaps();
			return FReply::Handled();
		}) [
			SNew(SImage).Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.MoveUp"))
			.ToolTipText(LOCTEXT("MoveDown", "Move Up"))
		];
	TSharedRef<SButton> DownButton = SNew(SButton)
		.OnClicked_Lambda([this, Line]()->FReply {
			int32 i = GetIndex(HeightMaps, Line);
			HeightMaps->RemoveSlot(Line);
			HeightMaps->InsertSlot(FMath::Min(HeightMaps->NumSlots(), i + 1)).AutoHeight().Padding(FMargin(0, 0, 0, 8))[ Line ];
			SaveHeightMaps();
			return FReply::Handled();
		}) [
			SNew(SImage)
			.Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.MoveDown"))
			.ToolTipText(LOCTEXT("MoveDown", "Move Down"))
		];
	
	Line->AddSlot().FillWidth(0.03);
	Line->AddSlot().FillWidth(0.07)[ LandscapeNameBlock ];
	Line->AddSlot().FillWidth(0.08)[ KindTextBlock ];
	Line->AddSlot().FillWidth(0.43)[ DescrBlock ];
	Line->AddSlot().FillWidth(0.08)[ PrecisionBlock ];
	Line->AddSlot().FillWidth(0.02)[ DownloadButton ];
	Line->AddSlot().FillWidth(0.02)[ ScaleDownButton ];
	Line->AddSlot().FillWidth(0.02)[ AdjustScaleOnlyButton ];
	Line->AddSlot().FillWidth(0.02)[ AdjustButton ];
	Line->AddSlot().FillWidth(0.02)[ RemoveButton ];
	Line->AddSlot().FillWidth(0.02)[ DownButton ];
	Line->AddSlot().FillWidth(0.02)[ UpButton ];
	Line->AddSlot().FillWidth(0.17);

	HeightMaps->AddSlot().AutoHeight().Padding(FMargin(0, 0, 0, 8))[ Line ];

	if (Save) SaveHeightMaps();
}

TSharedRef<SHorizontalBox> FLandscapeCombinatorModule::HeightMapLineAdder()
{	
	TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox);
	TSharedRef<SEditableText> AddMessageBlock = SNew(SEditableText).Text(LOCTEXT("AddMessageBlock", "Add a new heightmap line")).Font(RegularFont).IsReadOnly(true);
	TSharedRef<STextBlock> KindTextBlock = SNew(STextBlock).Text(RGEAltiText).Font(RegularFont);
	TSharedRef<SEditableTextBox> DescrBlock = SNew(SEditableTextBox).Text(DescriptionFromKind(RGEAltiText)).SelectAllTextWhenFocused(true).Font(RegularFont);

	TSharedRef<SComboBox<TSharedPtr<FText>>> KindSelector = SNew(SComboBox<TSharedPtr<FText>>)
		.OptionsSource(&Kinds)
		.OnSelectionChanged_Lambda([this, KindTextBlock, DescrBlock](TSharedPtr<FText> KindText, ESelectInfo::Type) {
			KindTextBlock->SetText(*KindText);
			DescrBlock->SetText(DescriptionFromKind(*KindText));
		})
		.OnGenerateWidget_Lambda([](TSharedPtr<FText> Text){
			return SNew(STextBlock).Text(*Text);
		}) [
			KindTextBlock
		];

	TSharedRef<SEditableTextBox> LandscapeNameBlock = SNew(SEditableTextBox).Text(LOCTEXT("LandscapeName", "Landscape Name")).SelectAllTextWhenFocused(true).Font(RegularFont);

	TSharedRef<SEditableTextBox> PrecisionBlock = SNew(SEditableTextBox).Text(LOCTEXT("Precision", "Precision in %")).SelectAllTextWhenFocused(true).Font(RegularFont);

	TSharedRef<SButton> AddButton = SNew(SButton)
		.OnClicked_Lambda([this, KindTextBlock, DescrBlock, Line, LandscapeNameBlock, PrecisionBlock]()->FReply {
			FText KindText = KindTextBlock->GetText();
			FString LandscapeName = LandscapeNameBlock->GetText().ToString().TrimStartAndEnd();
			FString Descr = DescrBlock->GetText().ToString().TrimStartAndEnd();
			int Precision = FCString::Atoi(*PrecisionBlock->GetText().ToString().TrimStartAndEnd());

			if (Precision <= 0 || Precision > 100) {
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("PrecisionInRange", "Precision must be an integer between 1 and 100"));
				return FReply::Unhandled();
			}

			AddHeightMapLine(LandscapeName, KindText, Descr, Precision, true);
			return FReply::Handled();
		}) [
			SNew(SImage)
			.Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.Add"))
			.ToolTipText(LOCTEXT("AddLine", "Add Heightmap Line"))
		];
		
	Line->AddSlot().AutoWidth().Padding(FMargin(40, 0, 10, 0))[ AddMessageBlock ];
	Line->AddSlot().AutoWidth().Padding(FMargin(0, 0, 10, 0))[ LandscapeNameBlock ];
	Line->AddSlot().AutoWidth().Padding(FMargin(0, 0, 10, 0))[ KindSelector ];
	Line->AddSlot().AutoWidth().Padding(FMargin(0, 0, 10, 0))[ DescrBlock ];
	Line->AddSlot().AutoWidth().Padding(FMargin(0, 0, 10, 0))[ PrecisionBlock ];
	Line->AddSlot().AutoWidth().Padding(FMargin(0, 0, 10, 0))[ AddButton ];

	return Line;
}

TSharedRef<SDockTab> FLandscapeCombinatorModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	SAssignNew(HeightMaps, SVerticalBox);
	SAssignNew(WorldWidthBlock, SEditableTextBox).SelectAllTextWhenFocused(true).Text(FText::FromString("40000")).Font(RegularFont);
	SAssignNew(WorldHeightBlock, SEditableTextBox).SelectAllTextWhenFocused(true).Text(FText::FromString("20000")).Font(RegularFont);
	SAssignNew(ZScaleBlock, SEditableTextBox).SelectAllTextWhenFocused(true).Text(FText::FromString("1")).Font(RegularFont);

	TSharedRef<STextBlock> WorldSizeBlock = SNew(STextBlock)
		.Text(LOCTEXT("WorldSizeBlock", "For the `Adjust` button, please enter the whole planet expected width and height in km, as well as the scale for the height (1 if you want real-world height): "))
		.Font(RegularFont);

		

	HeightMaps->AddSlot().AutoHeight().Padding(FMargin(0, 0, 0, 8))[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot().FillWidth(0.03)
		+SHorizontalBox::Slot().FillWidth(0.07)[
			SNew(SEditableText)
			.Text(LOCTEXT("LandscapeName", "Landscape Name"))
			.IsReadOnly(true)
			.Font(BoldFont)
		]
		+SHorizontalBox::Slot().FillWidth(0.08)[
			SNew(SEditableText)
			.Text(LOCTEXT("HMProvider", "Heightmap Provider"))
			.IsReadOnly(true)
			.Font(BoldFont)
		]
		+SHorizontalBox::Slot().FillWidth(0.43)[
			SNew(SEditableText)
			.Text(LOCTEXT("HMDetails", "Heightmap Details"))
			.IsReadOnly(true)
			.Font(BoldFont)
		]
		+SHorizontalBox::Slot().FillWidth(0.08)[
			SNew(SEditableText)
			.Text(LOCTEXT("Precision", "Precision (%)"))
			.IsReadOnly(true)
			.Font(BoldFont)
		]
		+SHorizontalBox::Slot().FillWidth(0.31)
	];
	LoadHeightMaps();
	
	TSharedRef<SDockTab> Tab = SNew(SDockTab).TabRole(ETabRole::NomadTab)[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(FMargin(0, 30, 0, 50))
			[
				SNew(STextBlock)
				.Text(FText::FromString("Landscape Combinator"))
				.Font(TitleFont)
			]
		+SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 40)) [
			HeightMaps.ToSharedRef()
		]
		+SVerticalBox::Slot().AutoHeight() [
			HeightMapLineAdder()
		].Padding(FMargin(0, 0, 0, 40))
		+SVerticalBox::Slot().AutoHeight() [
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot().Padding(FMargin(40, 0, 10, 0)).AutoWidth() [ WorldSizeBlock ]
			+SHorizontalBox::Slot().Padding(FMargin(0, 0, 10, 0)).AutoWidth() [ WorldWidthBlock.ToSharedRef() ]
			+SHorizontalBox::Slot().Padding(FMargin(0, 0, 10, 0)).AutoWidth() [ WorldHeightBlock.ToSharedRef() ]
			+SHorizontalBox::Slot().AutoWidth() [ ZScaleBlock.ToSharedRef() ]
		]
	];
	return Tab;
}

void FLandscapeCombinatorModule::SaveHeightMaps()
{
	TArray<HeightMapDetails> HeightMapList;
	
	for (int i = 1; i < HeightMaps->NumSlots(); i++) {
		TSharedRef<SHorizontalBox> Line = StaticCastSharedRef<SHorizontalBox>(HeightMaps->GetSlot(i).GetWidget());
		HeightMapDetails Details = {
			StaticCastSharedRef<SEditableText>(Line->GetSlot(1).GetWidget())->GetText().ToString(),
			StaticCastSharedRef<SEditableText>(Line->GetSlot(2).GetWidget())->GetText().ToString(),
			StaticCastSharedRef<SEditableText>(Line->GetSlot(3).GetWidget())->GetText().ToString(),
			FCString::Atoi(*StaticCastSharedRef<SEditableText>(Line->GetSlot(4).GetWidget())->GetText().ToString())
		};
		HeightMapList.Add(Details);
	}

	IPlatformFile::GetPlatformPhysical().CreateDirectory(*FPaths::ProjectSavedDir());
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Saving heightmap list to '%s'"), *HeightMapListProjectFile);
	FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*HeightMapListProjectFile);
	if (FileWriter)
	{
		*FileWriter << HeightMapList;
		FileWriter->Close();
	}
	else
	{
		UE_LOG(LogLandscapeCombinator, Error, TEXT("Failed to save the heightmap list to '%s'"), *HeightMapListProjectFile);
	}
}

void FLandscapeCombinatorModule::LoadHeightMaps()
{
	TArray<HeightMapDetails> HeightMapList;
	FArchive* FileReader = IFileManager::Get().CreateFileReader(*HeightMapListProjectFile);
	if (!FileReader) FileReader = IFileManager::Get().CreateFileReader(*HeightMapListPluginFile);

	if (FileReader) {
		*FileReader << HeightMapList;

		for (auto &Details : HeightMapList) {
			AddHeightMapLine(Details.LandscapeName, FText::FromString(Details.KindText), Details.Descr, Details.Precision, false);
		}

		FileReader->Close();
	}
	else {
		UE_LOG(LogLandscapeCombinator, Error, TEXT("Failed to load the heightmap list"));
	}

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
