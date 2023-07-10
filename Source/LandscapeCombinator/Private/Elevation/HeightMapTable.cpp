// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "Elevation/HeightMapTable.h"
#include "Elevation/HMInterface.h"
#include "Elevation/HMViewFinder1.h"
#include "Elevation/HMViewFinder3.h"
#include "Elevation/HMViewFinder15.h"
#include "Elevation/HMSwissAlti3D.h"
#include "Elevation/HMRGEAlti.h"
#include "Elevation/HMElevationAPI.h"
#include "Elevation/HMLocalFile.h"
#include "Elevation/HMReprojected.h"
#include "Elevation/HMURL.h"

#include "Utils/Logging.h"

#include "LandscapeCombinatorStyle.h"
#include "GlobalSettings.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

using namespace GlobalSettings;
	
FText KindChoice = LOCTEXT("HMInterface", "Please choose a heightmap provider");
FText EmptyDescr = LOCTEXT("HMDownloadDetails", "Coordinates for heightmap download");

FText ViewFinder15Text	= LOCTEXT("ViewFinder15", "Viewfinder Panoramas 15");
FText ViewFinder3Text	= LOCTEXT("ViewFinder3", "Viewfinder Panoramas 3");
FText ViewFinder1Text	= LOCTEXT("ViewFinder1", "Viewfinder Panoramas 1");
FText SwissAlti3DText	= LOCTEXT("swissAlti3D2", "swissAlti3D");
FText RGEAltiText		= LOCTEXT("RGEAlti", "RGE Alti France");
FText ElevationAPIText	= LOCTEXT("ElevationAPI", "Elevation API IGN 1m");
FText LocalFileText		= LOCTEXT("LocalFileKind", "Local File");
FText URLText			= LOCTEXT("URLKind", "Remote URL");

TArray<TSharedPtr<FText>> Kinds =
{
	MakeShareable(&ViewFinder15Text),
	MakeShareable(&ViewFinder3Text),
	MakeShareable(&ViewFinder1Text),
	MakeShareable(&SwissAlti3DText),
	MakeShareable(&RGEAltiText),
	MakeShareable(&ElevationAPIText),
	MakeShareable(&LocalFileText),
	MakeShareable(&URLText)
};

HeightMapTable::HeightMapTable() : SlateTable()
{
	HeightMapListProjectFileV0 = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()) / "HeightMapList";
	HeightMapListPluginFileV1  = FPaths::Combine(IPluginManager::Get().FindPlugin("LandscapeCombinator")->GetBaseDir(), "HeightMapListV1");
	HeightMapListProjectFileV1 = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()) / "HeightMapListV1";
	ColumnsSizes = { 0.07, 0.08, 0.43, 0.06, 0.08, 0.08, 0.2 }; // total: 100
}

TSharedRef<SWidget> HeightMapTable::Header()
{
	return SNew(SHorizontalBox)
	+SHorizontalBox::Slot().FillWidth(ColumnsSizes[0])[
		SNew(SEditableText)
		.Text(LOCTEXT("LandscapeLabel", "Landscape Label"))
		.IsReadOnly(true)
		.Font(FLandscapeCombinatorStyle::BoldFont())
	]
	+SHorizontalBox::Slot().FillWidth(ColumnsSizes[1])[
		SNew(SEditableText)
		.Text(LOCTEXT("HMProvider", "Heightmap Provider"))
		.IsReadOnly(true)
		.Font(FLandscapeCombinatorStyle::BoldFont())
	]
	+SHorizontalBox::Slot().FillWidth(ColumnsSizes[2])[
		SNew(SEditableText)
		.Text(LOCTEXT("HMDetails", "Heightmap Details"))
		.IsReadOnly(true)
		.Font(FLandscapeCombinatorStyle::BoldFont())
	]
	+SHorizontalBox::Slot().FillWidth(ColumnsSizes[3])[
		SNew(SEditableText)
		.Text(LOCTEXT("Precision", "Precision (%)"))
		.IsReadOnly(true)
		.Font(FLandscapeCombinatorStyle::BoldFont())
	]
	+SHorizontalBox::Slot().FillWidth(ColumnsSizes[4])[
		SNew(SEditableText)
		.Text(LOCTEXT("Reproject", "Reproject to EPSG:4326"))
		.IsReadOnly(true)
		.Font(FLandscapeCombinatorStyle::BoldFont())
	]
	+SHorizontalBox::Slot().FillWidth(ColumnsSizes[5])[
		SNew(SEditableText)
		.Text(LOCTEXT("Create", "Create Landscape"))
		.IsReadOnly(true)
		.Font(FLandscapeCombinatorStyle::BoldFont())
	]
	+SHorizontalBox::Slot().FillWidth(ColumnsSizes[6])
	;
}

TSharedRef<SWidget> HeightMapTable::Footer()
{	
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	TSharedRef<SEditableText> AddMessageBlock = SNew(SEditableText).Text(LOCTEXT("AddMessageBlock", "Add a new heightmap line")).Font(FLandscapeCombinatorStyle::RegularFont()).IsReadOnly(true);
	TSharedRef<STextBlock> KindTextBlock = SNew(STextBlock).Text(RGEAltiText).Font(FLandscapeCombinatorStyle::RegularFont());
	TSharedRef<SEditableTextBox> DescrBlock = SNew(SEditableTextBox).Text(DescriptionFromKind(RGEAltiText)).SelectAllTextWhenFocused(true).Font(FLandscapeCombinatorStyle::RegularFont());

	TSharedRef<SCheckBox> ReprojectionCheckBox = SNew(SCheckBox).ToolTipText(LOCTEXT("Reproject", "Reproject to EPSG:4326 (check it if unsure)")).IsChecked(false);

	TSharedRef<SComboBox<TSharedPtr<FText>>> KindSelector = SNew(SComboBox<TSharedPtr<FText>>)
		.OptionsSource(&Kinds)
		.OnSelectionChanged_Lambda([this, KindTextBlock, DescrBlock, ReprojectionCheckBox](TSharedPtr<FText> KindText, ESelectInfo::Type) {
			KindTextBlock->SetText(*KindText);
			DescrBlock->SetText(DescriptionFromKind(*KindText));
			ReprojectionCheckBox->SetIsChecked(ECheckBoxState::Unchecked);
			ReprojectionCheckBox->SetEnabled(KindSupportsReprojection(*KindText));
			if (KindSupportsReprojection(*KindText))
			{
				ReprojectionCheckBox->SetVisibility(EVisibility::Visible);
			}
			else
			{
				ReprojectionCheckBox->SetVisibility(EVisibility::Collapsed);
			}
		})
		.OnGenerateWidget_Lambda([](TSharedPtr<FText> Text){
			return SNew(STextBlock).Text(*Text);
		}) [
			KindTextBlock
		];

	TSharedRef<SEditableTextBox> LandscapeLabelBlock =
		SNew(SEditableTextBox)
		.Text(LOCTEXT("LandscapeLabel", "Landscape Label"))
		.SelectAllTextWhenFocused(true)
		.Font(FLandscapeCombinatorStyle::RegularFont());

	TSharedRef<SEditableTextBox> PrecisionBlock = SNew(SEditableTextBox).Text(LOCTEXT("Precision", "Precision in %")).SelectAllTextWhenFocused(true).Font(FLandscapeCombinatorStyle::RegularFont());

	TSharedRef<SButton> AddButton = SNew(SButton)
		.OnClicked_Lambda([this, KindTextBlock, DescrBlock, Row, LandscapeLabelBlock, PrecisionBlock, ReprojectionCheckBox]()->FReply {
			FText KindText = KindTextBlock->GetText();
			FString LandscapeLabel = LandscapeLabelBlock->GetText().ToString().TrimStartAndEnd();
			FString Descr = DescrBlock->GetText().ToString().TrimStartAndEnd();
			int Precision = FCString::Atoi(*PrecisionBlock->GetText().ToString().TrimStartAndEnd());

			if (Precision <= 0 || Precision > 100) {
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("PrecisionInRange", "Precision must be an integer between 1 and 100"));
				return FReply::Unhandled();
			}

			AddHeightMapRow(LandscapeLabel, KindText, Descr, Precision, ReprojectionCheckBox->GetCheckedState() == ECheckBoxState::Checked, true);
			return FReply::Handled();
		}) [
			SNew(SImage)
			.Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.Add"))
			.ToolTipText(LOCTEXT("AddLine", "Add Heightmap Row"))
		];
		
	Row->AddSlot().FillWidth(0.10).Padding(FMargin(0, 0, 10, 0))[ AddMessageBlock ];
	Row->AddSlot().FillWidth(0.08).Padding(FMargin(0, 0, 10, 0))[ LandscapeLabelBlock ];
	Row->AddSlot().FillWidth(0.10).Padding(FMargin(0, 0, 10, 0))[ KindSelector ];
	Row->AddSlot().FillWidth(0.40).Padding(FMargin(0, 0, 10, 0))[ DescrBlock ];
	Row->AddSlot().FillWidth(0.06).Padding(FMargin(0, 0, 10, 0))[ PrecisionBlock ];
	Row->AddSlot().FillWidth(0.01).Padding(FMargin(0, 0, 10, 0))[ ReprojectionCheckBox ];
	Row->AddSlot().FillWidth(0.02).Padding(FMargin(0, 0, 10, 0))[ AddButton ];
	Row->AddSlot().FillWidth(0.18);

	return Row;
}

FString LandscapeLabelFromRow(TSharedRef<SHorizontalBox> Row)
{
	return StaticCastSharedRef<SEditableText>(Row->GetSlot(0).GetWidget())->GetText().ToString();
}

void HeightMapTable::OnRemove(TSharedRef<SHorizontalBox> Row)
{
	Interfaces.Remove(LandscapeLabelFromRow(Row));
	Landscapes.Remove(FName(*LandscapeLabelFromRow(Row)));
}

HMInterface* HeightMapTable::InterfaceFromKind(FString LandscapeLabel0, const FText& KindText0, FString Descr0, int Precision0)
{
	if (KindText0.EqualTo(ViewFinder15Text)) return new HMViewFinder15(LandscapeLabel0, KindText0, Descr0, Precision0);
	else if (KindText0.EqualTo(ViewFinder3Text)) return new HMViewFinder3(LandscapeLabel0, KindText0, Descr0, Precision0);
	else if (KindText0.EqualTo(ViewFinder1Text)) return new HMViewFinder1(LandscapeLabel0, KindText0, Descr0, Precision0);
	else if (KindText0.EqualTo(SwissAlti3DText)) return new HMSwissAlti3D(LandscapeLabel0, KindText0, Descr0, Precision0);
	else if (KindText0.EqualTo(RGEAltiText)) return new HMRGEAlti(LandscapeLabel0, KindText0, Descr0, Precision0);
	else if (KindText0.EqualTo(ElevationAPIText)) return new HMElevationAPI(LandscapeLabel0, KindText0, Descr0, Precision0);
	else if (KindText0.EqualTo(LocalFileText)) return new HMLocalFile(LandscapeLabel0, KindText0, Descr0, Precision0);
	else if (KindText0.EqualTo(URLText)) return new HMURL(LandscapeLabel0, KindText0, Descr0, Precision0);
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

FText HeightMapTable::DescriptionFromKind(const FText& KindText0)
{
	if (KindText0.EqualTo(ViewFinder15Text)) return LOCTEXT("ViewFinder15Descr", "Enter the comma-separated list of rectangles (e.g. 15-A, 15-B, 15-G, 15-H) from http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org15.htm");
	else if (KindText0.EqualTo(ViewFinder3Text)) return LOCTEXT("ViewFinder3Descr", "Enter the comma-separated list of rectangles (e.g. M31, M32) from http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org3.htm");
	else if (KindText0.EqualTo(ViewFinder1Text)) return LOCTEXT("ViewFinder1Descr", "Enter the comma-separated list of rectangles (e.g. M31, M32) from http://viewfinderpanoramas.org/Coverage%20map%20viewfinderpanoramas_org1.htm");
	else if (KindText0.EqualTo(SwissAlti3DText)) return LOCTEXT("SwissAlti3DDescr", "Enter C:\\Path\\To\\ListOfLinks.csv (see README for more details)");
	else if (KindText0.EqualTo(RGEAltiText)) return LOCTEXT("RGEAltiDescr", "Enter MinLong,MaxLong,MinLat,MaxLat (and optionally ,Width,Height) in EPSG 2154 coordinates");
	else if (KindText0.EqualTo(ElevationAPIText)) return LOCTEXT("ElevationAPIDescr", "Enter MinLong,MaxLong,MinLat,MaxLat in EPSG 4326 coordinates");
	else if (KindText0.EqualTo(LocalFileText)) return LOCTEXT("LocalFileDescr", "Enter your C:\\Path\\To\\MyHeightmap.tif in GeoTIFF format");
	else if (KindText0.EqualTo(URLText)) return LOCTEXT("URLDescr", "Enter URL to a heightmap in GeoTIFF format");
	else return FText();
}

bool HeightMapTable::KindSupportsReprojection(const FText& KindText0) {
	return KindText0.EqualTo(RGEAltiText) || KindText0.EqualTo(ElevationAPIText) || KindText0.EqualTo(LocalFileText) || KindText0.EqualTo(URLText);
}

void HeightMapTable::Save()
{
	TArray<HeightMapDetailsV1> HeightMapList;
	
	for (int i = 0; i < Rows->NumSlots(); i++) {
		TSharedRef<SHorizontalBox> Row = StaticCastSharedRef<SHorizontalBox>(Rows->GetSlot(i).GetWidget());
		HeightMapDetailsV1 Details = {
			LandscapeLabelFromRow(Row),
			StaticCastSharedRef<SEditableText>(Row->GetSlot(1).GetWidget())->GetText().ToString(),
			StaticCastSharedRef<SEditableText>(Row->GetSlot(2).GetWidget())->GetText().ToString(),
			FCString::Atoi(*StaticCastSharedRef<SEditableText>(Row->GetSlot(3).GetWidget())->GetText().ToString()),
			StaticCastSharedRef<SEditableText>(Row->GetSlot(4).GetWidget())->GetText().ToString().Equals("Yes")
		};
		HeightMapList.Add(Details);
	}

	IPlatformFile::GetPlatformPhysical().CreateDirectory(*FPaths::ProjectSavedDir());
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Saving heightmap list to '%s'"), *HeightMapListProjectFileV1);
	FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*HeightMapListProjectFileV1);
	if (FileWriter)
	{
		*FileWriter << HeightMapList;
		FileWriter->Close();
	}
	else
	{
		UE_LOG(LogLandscapeCombinator, Error, TEXT("Failed to save the heightmap list to '%s'"), *HeightMapListProjectFileV1);
	}
}

void HeightMapTable::LoadFrom(FString FilePath)
{
	FArchive* FileReader = IFileManager::Get().CreateFileReader(*FilePath);
	if (FileReader)
	{
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Loading heightmap list from %s"), *FilePath);
	}

	if (FileReader)
	{
		TArray<HeightMapDetailsV1> HeightMapList;
		*FileReader << HeightMapList;

		for (auto &Details : HeightMapList)
		{
			AddHeightMapRow(Details.LandscapeLabel, FText::FromString(Details.KindText), Details.Descr, Details.Precision, Details.Reproject, false);
		}

		FileReader->Close();
	}
	else
	{
		UE_LOG(LogLandscapeCombinator, Error, TEXT("Failed to load the heightmap list from %s"), *FilePath);
	}
}

void HeightMapTable::Load()
{
	FArchive* FileReader = IFileManager::Get().CreateFileReader(*HeightMapListProjectFileV1);
	if (FileReader)
	{
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Loading heightmap list from %s"), *HeightMapListProjectFileV1);
	} else
	{
		FileReader = IFileManager::Get().CreateFileReader(*HeightMapListPluginFileV1);
		if (FileReader)
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Loading heightmap list from %s"), *HeightMapListPluginFileV1);
	}

	if (FileReader)
	{
		TArray<HeightMapDetailsV1> HeightMapList;
		*FileReader << HeightMapList;

		for (auto &Details : HeightMapList)
		{
			AddHeightMapRow(Details.LandscapeLabel, FText::FromString(Details.KindText), Details.Descr, Details.Precision, Details.Reproject, false);
		}

		FileReader->Close();
	}
	else
	{
		FileReader = IFileManager::Get().CreateFileReader(*HeightMapListProjectFileV0);
		if (FileReader)
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Loading heightmap list from %s"), *HeightMapListProjectFileV0);
			TArray<HeightMapDetailsV0> HeightMapList;
			*FileReader << HeightMapList;

			for (auto &Details : HeightMapList)
			{
				AddHeightMapRow(Details.LandscapeLabel, FText::FromString(Details.KindText), Details.Descr, Details.Precision, false, false);
			}

			FileReader->Close();
		}
		else
		{
			UE_LOG(LogLandscapeCombinator, Error, TEXT("Failed to load heightmap list"));
		}
	}
}

void HeightMapTable::AddHeightMapRow(FString LandscapeLabel, const FText& KindText, FString Descr, int Precision, bool bReproject, bool bSave)
{
	if (Interfaces.Contains(LandscapeLabel))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("AddHeightMapLineError", "There already exists a landscape labeled {0} in the heightmaps table."),
				FText::FromString(LandscapeLabel)
			)
		); 
		return;
	}

	HMInterface *Interface = InterfaceFromKind(LandscapeLabel, KindText, Descr, Precision);

	if (!Interface || !Interface->Initialize())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("InitializationError", "There was an error while initializing {0}. Please refer to other error messages."),
				FText::FromString(LandscapeLabel)
			)
		); 
		return;
	}

	if (bReproject) {
		Interface = new HMReprojected(LandscapeLabel, KindText, Descr, Precision, Interface);
		if (!Interface || !Interface->Initialize()) {
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(
					LOCTEXT("InitializationError", "There was an error while initializing {0}. Please refer to other error messages."),
					FText::FromString(LandscapeLabel)
				)
			); 
			return;
		}
	}

	Interfaces.Add(LandscapeLabel, Interface);
	Landscapes.Add(FName(*LandscapeLabel));

	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	TSharedRef<SEditableText> LandscapeLabelBlock = SNew(SEditableText).Text(FText::FromString(LandscapeLabel)).IsReadOnly(true).Font(FLandscapeCombinatorStyle::RegularFont());
	TSharedRef<SEditableText> KindTextBlock = SNew(SEditableText).Text(KindText).IsReadOnly(true).Font(FLandscapeCombinatorStyle::RegularFont());
	TSharedRef<SEditableText> DescrBlock = SNew(SEditableText).Text(FText::FromString(Descr)).IsReadOnly(true).Font(FLandscapeCombinatorStyle::RegularFont());
	TSharedRef<SEditableText> PrecisionBlock = SNew(SEditableText).Text(FText::FromString(FString::Format(TEXT("{0}%"), { Precision }))).IsReadOnly(true).Font(FLandscapeCombinatorStyle::RegularFont());
	TSharedRef<SEditableText> ReprojectBlock = SNew(SEditableText).Text(FText::FromString(bReproject ? "Yes" : "No")).IsReadOnly(true).Font(FLandscapeCombinatorStyle::RegularFont());

	TSharedRef<SButton> DownloadButton = SNew(SButton)
		.OnClicked_Lambda([Interface]()->FReply {
			return Interface->DownloadHeightMaps([Interface](bool bWasSuccessful) {
				if (bWasSuccessful)
				{
					FMessageDialog::Open(EAppMsgType::Ok,
						FText::Format(
							LOCTEXT("DownloadSuccess", "Download for Landscape {0} was successful. You can now press the `Convert` button."),
							FText::FromString(Interface->LandscapeLabel))
					); 
				}
				else
				{
					FMessageDialog::Open(EAppMsgType::Ok,
						FText::Format(
							LOCTEXT("DownloadError", "There was an error while downloading heightmaps for Landscape {0}. Please check the Output Log."),
							FText::FromString(Interface->LandscapeLabel))
					); 

				}
			});
		}) [
			SNew(SImage)
			.Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.Download"))
			.ToolTipText(LOCTEXT("Download", "Download Heightmaps"))
		];

	if (KindText.EqualTo(LocalFileText)) DownloadButton->SetEnabled(false);

	TSharedRef<SButton> ConvertButton = SNew(SButton)
		.OnClicked_Lambda([Interface]()->FReply {
			return Interface->ConvertHeightMaps([Interface](bool bWasSuccessful) {
				if (bWasSuccessful)
				{
					FMessageDialog::Open(EAppMsgType::Ok,
						FText::Format(
							LOCTEXT("ConvertSuccess", "Heightmap conversions for Landscape {0} were successful. You can now press the `Import` button."),
							FText::FromString(Interface->LandscapeLabel))
					); 
				}
				else
				{
					FMessageDialog::Open(EAppMsgType::Ok,
						FText::Format(
							LOCTEXT("ConvertError", "There was an error while converting heightmaps for Landscape {0}. Please check the Output Log."),
							FText::FromString(Interface->LandscapeLabel))
					); 

				}
			});
		}) [
			SNew(SImage)
			.Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.Convert"))
			.ToolTipText(LOCTEXT("Convert", "Convert Heightmaps to PNG"))
		];

	TSharedRef<SButton> AdjustButton = SNew(SButton)
		.OnClicked_Lambda([this, Interface]()->FReply {
			WorldParametersV1 Params;
			if (GetWorldParameters(Params))
				return Interface->AdjustLandscape(Params.WorldWidthKm, Params.WorldHeightKm, Params.ZScale, Params.WorldOriginX, Params.WorldOriginY);
			else
				return FReply::Unhandled();
		}) [
			SNew(SImage).Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.Adjust"))
			.ToolTipText(LOCTEXT("Adjust", "Adjust scale and set landscape position relative to the world.\n(Only click this if you already imported the landscape and changed the global settings afterwards.)"))
		];

	TSharedRef<SButton> ImportButton = SNew(SButton)
		.OnClicked_Lambda([this, Interface]()->FReply {
			return Interface->ImportHeightMaps();
		}) [
			SNew(SImage).Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.Import"))
			.ToolTipText(LOCTEXT("ImportHeightMaps", "Create Landscape"))
		];

	TSharedRef<SButton> CreateLandscapeButton = SNew(SButton)
		.OnClicked_Lambda([this, Interface]()->FReply {
			WorldParametersV1 Params;
			if (GetWorldParameters(Params))
				return Interface->CreateLandscape(Params.WorldWidthKm, Params.WorldHeightKm, Params.ZScale, Params.WorldOriginX, Params.WorldOriginY, nullptr);
			else
				return FReply::Unhandled();
		}) [
			SNew(SImage).Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.Landscape"))
			.ToolTipText(LOCTEXT("CreateLandscape", "Download height maps, convert them, and create landscape"))
		];
	
	Row->AddSlot().FillWidth(ColumnsSizes[0])[ LandscapeLabelBlock ];
	Row->AddSlot().FillWidth(ColumnsSizes[1])[ KindTextBlock ];
	Row->AddSlot().FillWidth(ColumnsSizes[2])[ DescrBlock ];
	Row->AddSlot().FillWidth(ColumnsSizes[3])[ PrecisionBlock ];
	Row->AddSlot().FillWidth(ColumnsSizes[4])[ ReprojectBlock ];
	//Row->AddSlot().FillWidth(0.02)[ DownloadButton ];
	//Row->AddSlot().FillWidth(0.02)[ ConvertButton ];
	//Row->AddSlot().FillWidth(0.02)[ ImportButton ];
	Row->AddSlot().FillWidth(ColumnsSizes[5])[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot().AutoWidth() [ CreateLandscapeButton ]
		+SHorizontalBox::Slot().AutoWidth() [ AdjustButton ]
	];

	AddRow(Row, true, bSave, ColumnsSizes[6]);
}