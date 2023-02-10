#include "Road/RoadTable.h"
#include "Road/RoadBuilder.h"
#include "Road/RoadBuilderShortQuery.h"
#include "Road/RoadBuilderOverpass.h"
#include "Road/RoadBuilderSelector.h"
#include "Road/RoadBuilderAll.h"
#include "Utils/Logging.h"
#include "Utils/Concurrency.h"
#include "LandscapeCombinatorStyle.h"
#include "GlobalSettings.h"

#include "Interfaces/IPluginManager.h"
#include "GenericPlatform/GenericPlatformFile.h" 

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

using namespace GlobalSettings;

FText AllRoadsText				= LOCTEXT("AllRoads", "All Roads");
FText RoadSelectorText			= LOCTEXT("RoadSelection", "Road Selection");
FText RoadLocalFileText			= LOCTEXT("LocalFileKind", "Local File");
FText OverpassApiText			= LOCTEXT("OverpassKind", "Overpass Compact URL ");
FText OverpassShortQueryText	= LOCTEXT("OverpassShortQuery", "Overpass Short Query");

TArray<TSharedPtr<FText>> RoadSources =
{
	MakeShareable(&AllRoadsText),
	MakeShareable(&RoadSelectorText),
	MakeShareable(&RoadLocalFileText),
	MakeShareable(&OverpassApiText),
	MakeShareable(&OverpassShortQueryText),
};

RoadTable::RoadTable(HeightMapTable* HMTable0) : SlateTable()
{
	ColumnsSizes = { 0.07, 0.41, 0.07, 0.07, 0.08, 0.44 };
	HMTable = HMTable0;
	RoadListPluginFileV1  = FPaths::Combine(IPluginManager::Get().FindPlugin("LandscapeCombinator")->GetBaseDir(), "RoadListV1");
	RoadListProjectFileV1 = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()) / "RoadListV1";
}


bool SerializeArray(FArchive& Ar, TArray<FRoadBuilder*>& RoadBuilders)
{
	int32 Size = RoadBuilders.Num();
	Ar << Size;
	if (Ar.IsLoading())
	{
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Loading an array with %d road builders"), Size);
		RoadBuilders.SetNum(Size);
	}
	for (int32 i = 0; i < Size; i++)
	{
		int Source;
		if (!Ar.IsLoading()) Source = RoadBuilders[i]->SourceKind;

		Ar << Source;

		if (Ar.IsLoading())
		{
			if (Source == ESourceKind::LocalFile) RoadBuilders[i] = new FRoadBuilder();
			else if (Source == ESourceKind::OverpassAPI) RoadBuilders[i] = new FRoadBuilderOverpass();
			else if (Source == ESourceKind::OverpassShortQuery) RoadBuilders[i] = new FRoadBuilderShortQuery();
			else if (Source == ESourceKind::RoadSelector) RoadBuilders[i] = new FRoadBuilderSelector();
			else if (Source == ESourceKind::AllRoads) RoadBuilders[i] = new FRoadBuilderAll();
			else {
				UE_LOG(LogLandscapeCombinator, Error, TEXT("Error while loading road list, found unknown source kind %d"), Source);
				return false;
			}
		}
		
		if (Source == ESourceKind::LocalFile) Ar << *(RoadBuilders[i]);
		else if (Source == ESourceKind::OverpassAPI) Ar << *static_cast<FRoadBuilderOverpass*>(RoadBuilders[i]);
		else if (Source == ESourceKind::OverpassShortQuery) Ar << *static_cast<FRoadBuilderShortQuery*>(RoadBuilders[i]);
		else if (Source == ESourceKind::RoadSelector) Ar << *static_cast<FRoadBuilderSelector*>(RoadBuilders[i]);
		else if (Source == ESourceKind::AllRoads) Ar << *static_cast<FRoadBuilderAll*>(RoadBuilders[i]);
		else
		{
			UE_LOG(LogLandscapeCombinator, Error, TEXT("Error while serializing road list, found unknown source kind %d"), Source);
			return false;
		}
	}
	return true;
}


void RoadTable::Save()
{
	IPlatformFile::GetPlatformPhysical().CreateDirectory(*FPaths::ProjectSavedDir());
	UE_LOG(LogLandscapeCombinator, Log, TEXT("Saving road list to '%s'"), *RoadListProjectFileV1);
	FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*RoadListProjectFileV1);
	if (FileWriter)
	{
		SerializeArray(*FileWriter, RoadBuilders); // should always return true while in Saving mode
		if (FileWriter->Close()) return;
	}

	UE_LOG(LogLandscapeCombinator, Error, TEXT("Failed to save the road list to '%s'"), *RoadListProjectFileV1);
}

void RoadTable::Load()
{
	FArchive* FileReader = IFileManager::Get().CreateFileReader(*RoadListProjectFileV1);
	if (FileReader)
	{
		UE_LOG(LogLandscapeCombinator, Log, TEXT("Loading road builders list from %s"), *RoadListProjectFileV1);
	}
	else
	{
		FileReader = IFileManager::Get().CreateFileReader(*RoadListPluginFileV1);
		if (FileReader)
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Loading road builders list from %s"), *RoadListPluginFileV1);
	}

	if (FileReader)
	{
		TArray<FRoadBuilder*> RoadBuildersTemp;
		
		// we load in RoadBuildersTemp rather than RoadBuilders because AddRoadRow will add the elements to RoadBuilders
		if (SerializeArray(*FileReader, RoadBuildersTemp))
		{
			UE_LOG(LogLandscapeCombinator, Log, TEXT("Found %d road builders"), RoadBuildersTemp.Num());

			for (auto &RoadBuilder : RoadBuildersTemp)
			{
				RoadBuilder->HMTable = HMTable;
				UE_LOG(LogLandscapeCombinator, Log, TEXT("Adding road builder for landscape: %s"), *(RoadBuilder->LandscapeLabel));
				AddRoadRow(RoadBuilder, false);
			}
		}

		if (FileReader->Close()) return;
	}

	UE_LOG(LogLandscapeCombinator, Error, TEXT("Failed to load the road builders list"));
}

TSharedRef<SWidget> RoadTable::Header()
{
	return SNew(SHorizontalBox)
	+SHorizontalBox::Slot().FillWidth(ColumnsSizes[0])
	[
		SNew(SEditableText)
		.Text(LOCTEXT("TargetLandscape", "Target Landscape"))
		.IsReadOnly(true)
		.Font(FLandscapeCombinatorStyle::BoldFont())
	]
	+SHorizontalBox::Slot().FillWidth(ColumnsSizes[1])
	[
		SNew(SEditableText)
		.Text(LOCTEXT("Details", "Details"))
		.IsReadOnly(true)
		.Font(FLandscapeCombinatorStyle::BoldFont())
	]
	//+SHorizontalBox::Slot().FillWidth(ColumnsSizes[2])
	//[
	//	SNew(SEditableText)
	//	.Text(LOCTEXT("LandscapeLayer", "Landscape Layer"))
	//	.IsReadOnly(true)
	//	.Font(FLandscapeCombinatorStyle::BoldFont())
	//]
	//+SHorizontalBox::Slot().FillWidth(ColumnsSizes[3])
	//[
	//	SNew(SEditableText)
	//	.Text(LOCTEXT("RoadWidth", "Road Width (m)"))
	//	.IsReadOnly(true)
	//	.Font(FLandscapeCombinatorStyle::BoldFont())
	//]
	+SHorizontalBox::Slot().FillWidth(ColumnsSizes[4])
	[
		SNew(SEditableText)
		.Text(LOCTEXT("Create", "Create Roads"))
		.IsReadOnly(true)
		.Font(FLandscapeCombinatorStyle::BoldFont())
	]
	+SHorizontalBox::Slot().FillWidth(ColumnsSizes[5])
	;
}

void RoadTable::AddRoadRow(FRoadBuilder* RoadBuilder, bool bSave)
{
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	RowToBuilder.Add(Row, RoadBuilder);
	RoadBuilders.Add(RoadBuilder);
	TSharedRef<SEditableText> LandscapeLabelBlock = SNew(SEditableText).Text(FText::FromString(RoadBuilder->LandscapeLabel)).IsReadOnly(true).Font(FLandscapeCombinatorStyle::RegularFont());
	TSharedRef<SEditableText> DetailsBlock = SNew(SEditableText).Text(FText::FromString(RoadBuilder->DetailsString())).IsReadOnly(true).Font(FLandscapeCombinatorStyle::RegularFont());
	TSharedRef<SEditableText> LayerNameBlock = SNew(SEditableText).Text(FText::FromString(RoadBuilder->LayerName)).IsReadOnly(true).Font(FLandscapeCombinatorStyle::RegularFont());
	TSharedRef<SEditableText> RoadWidthBlock = SNew(SEditableText).Text(FText::AsNumber(RoadBuilder->RoadWidth)).IsReadOnly(true).Font(FLandscapeCombinatorStyle::RegularFont());

	TSharedRef<SButton> CreateRoadsButton = SNew(SButton)
		.OnClicked_Lambda([this, RoadBuilder]()->FReply {
			WorldParametersV1 Params;
			if (GetWorldParameters(Params))
				RoadBuilder->AddRoads(Params.WorldWidthKm, Params.WorldHeightKm, Params.ZScale, Params.WorldOriginX, Params.WorldOriginY);
			return FReply::Handled();
		}) [
			SNew(SImage).Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.AddRoad"))
			.ToolTipText(LOCTEXT("AddRoads", "Add Roads to the target landscape"))
		];
	
	Row->AddSlot().FillWidth(ColumnsSizes[0])[ LandscapeLabelBlock ];
	Row->AddSlot().FillWidth(ColumnsSizes[1])[ DetailsBlock ];
	//Row->AddSlot().FillWidth(ColumnsSizes[2])[ LayerNameBlock ];
	//Row->AddSlot().FillWidth(ColumnsSizes[3])[ RoadWidthBlock ];
	Row->AddSlot().FillWidth(ColumnsSizes[4])[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot().AutoWidth() [ CreateRoadsButton ]
	];
	AddRow(Row, true, bSave, ColumnsSizes[5]);
}

TSharedRef<SWidget> RoadTable::Footer()
{
	FText FirstLandscape = HMTable->Landscapes.IsEmpty() ? FText::FromString("") : FText::FromName(HMTable->Landscapes[0]);
	TSharedRef<SHorizontalBox> Result = SNew(SHorizontalBox);
	TSharedRef<SEditableText> LandscapeChoiceBlock = SNew(SEditableText).Text(LOCTEXT("LandscapeChoiceBlock", "Landscape Target")).Font(FLandscapeCombinatorStyle::RegularFont()).IsReadOnly(true);
	TSharedRef<STextBlock> LandscapeBlock = SNew(STextBlock).Text(FirstLandscape).Font(FLandscapeCombinatorStyle::RegularFont());
	TSharedRef<SEditableText> SourceChoiceBlock = SNew(SEditableText).Text(LOCTEXT("RoadSource", "Road Source")).Font(FLandscapeCombinatorStyle::RegularFont()).IsReadOnly(true);
	TSharedRef<STextBlock> SourceBlock = SNew(STextBlock).Text(AllRoadsText).Font(FLandscapeCombinatorStyle::RegularFont());
	TSharedRef<SEditableTextBox> LayerNameBlock = SNew(SEditableTextBox).Text(LOCTEXT("LandscapeLayer", "Landscape Layer Name (to paint the roads)")).SelectAllTextWhenFocused(true).Font(FLandscapeCombinatorStyle::RegularFont());
	TSharedRef<SEditableTextBox> RoadWidthBlock = SNew(SEditableTextBox).Text(LOCTEXT("RoadWidth", "Road width (m)")).SelectAllTextWhenFocused(true).Font(FLandscapeCombinatorStyle::RegularFont());
	
	TSharedRef<SEditableTextBox> OverpassBlock =
		SNew(SEditableTextBox).Text(LOCTEXT("OverpassQuery", "Overpass Query Compact URL."))
		.SelectAllTextWhenFocused(true)
		.Font(FLandscapeCombinatorStyle::RegularFont())
		.Visibility_Lambda([SourceBlock]() { return SourceBlock->GetText().EqualTo(OverpassApiText) ? EVisibility::Visible : EVisibility::Collapsed; })
	;
	TSharedRef<SEditableTextBox> OverpassShortBlock =
		SNew(SEditableTextBox).Text(LOCTEXT("OverpasShort", "\"way\"=\"highway\""))
		.SelectAllTextWhenFocused(true)
		.Font(FLandscapeCombinatorStyle::RegularFont())
		.Visibility_Lambda([SourceBlock]() { return SourceBlock->GetText().EqualTo(OverpassShortQueryText) ? EVisibility::Visible : EVisibility::Collapsed; })
	;
	TSharedRef<SEditableTextBox> XmlPathBlock =
		SNew(SEditableTextBox).Text(LOCTEXT("XMLPath", "Path to the XML file containing OSM data."))
		.SelectAllTextWhenFocused(true)
		.Font(FLandscapeCombinatorStyle::RegularFont())
		.Visibility_Lambda([SourceBlock]() { return SourceBlock->GetText().EqualTo(RoadLocalFileText) ? EVisibility::Visible : EVisibility::Collapsed; })
	;

	TSharedRef<SEditableText> MotorwayLabel		= SNew(SEditableText).Text(LOCTEXT("Motorway", "Motorway")).Font(FLandscapeCombinatorStyle::RegularFont()).IsReadOnly(true);
	TSharedRef<SEditableText> TrunkLabel		= SNew(SEditableText).Text(LOCTEXT("Trunk", "Trunk")).Font(FLandscapeCombinatorStyle::RegularFont()).IsReadOnly(true);
	TSharedRef<SEditableText> PrimaryLabel		= SNew(SEditableText).Text(LOCTEXT("Primary", "Primary")).Font(FLandscapeCombinatorStyle::RegularFont()).IsReadOnly(true);
	TSharedRef<SEditableText> SecondaryLabel	= SNew(SEditableText).Text(LOCTEXT("Secondary", "Secondary")).Font(FLandscapeCombinatorStyle::RegularFont()).IsReadOnly(true);
	TSharedRef<SEditableText> TertiaryLabel		= SNew(SEditableText).Text(LOCTEXT("Tertiary", "Tertiary")).Font(FLandscapeCombinatorStyle::RegularFont()).IsReadOnly(true);
	TSharedRef<SEditableText> UnclassifiedLabel	= SNew(SEditableText).Text(LOCTEXT("Unclassified", "Unclassified")).Font(FLandscapeCombinatorStyle::RegularFont()).IsReadOnly(true);
	TSharedRef<SEditableText> ResidentialLabel	= SNew(SEditableText).Text(LOCTEXT("Residential", "Residential")).Font(FLandscapeCombinatorStyle::RegularFont()).IsReadOnly(true);
	TSharedRef<SCheckBox> MotorwayCheckBox		= SNew(SCheckBox).IsChecked(true);
	TSharedRef<SCheckBox> TrunkCheckBox			= SNew(SCheckBox).IsChecked(true);
	TSharedRef<SCheckBox> PrimaryCheckBox		= SNew(SCheckBox).IsChecked(true);
	TSharedRef<SCheckBox> SecondaryCheckBox		= SNew(SCheckBox).IsChecked(true);
	TSharedRef<SCheckBox> TertiaryCheckBox		= SNew(SCheckBox).IsChecked(true);
	TSharedRef<SCheckBox> UnclassifiedCheckBox	= SNew(SCheckBox).IsChecked(true);
	TSharedRef<SCheckBox> ResidentialCheckBox	= SNew(SCheckBox).IsChecked(true);
	TSharedRef<SHorizontalBox> RoadSelectorChoices =
		SNew(SHorizontalBox)
		.Visibility_Lambda([SourceBlock]() { return SourceBlock->GetText().EqualTo(RoadSelectorText) ? EVisibility::Visible : EVisibility::Collapsed; })
		+SHorizontalBox::Slot().AutoWidth() [ MotorwayCheckBox ]
		+SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0, 0, 15, 0)) [ MotorwayLabel ]
		+SHorizontalBox::Slot().AutoWidth() [ TrunkCheckBox ]
		+SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0, 0, 15, 0)) [ TrunkLabel ]
		+SHorizontalBox::Slot().AutoWidth() [ PrimaryCheckBox ]
		+SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0, 0, 15, 0)) [ PrimaryLabel ]
		+SHorizontalBox::Slot().AutoWidth() [ SecondaryCheckBox ]
		+SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0, 0, 15, 0)) [ SecondaryLabel ]
		+SHorizontalBox::Slot().AutoWidth() [ TertiaryCheckBox ]
		+SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0, 0, 15, 0)) [ TertiaryLabel ]
		+SHorizontalBox::Slot().AutoWidth() [ UnclassifiedCheckBox ]
		+SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0, 0, 15, 0)) [ UnclassifiedLabel ]
		+SHorizontalBox::Slot().AutoWidth() [ ResidentialCheckBox ]
		+SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0, 0, 15, 0)) [ ResidentialLabel ]
	;

	TSharedRef<SComboBox<FName>> LandscapeSelector = SNew(SComboBox<FName>)
		.OptionsSource(&HMTable->Landscapes)
		.OnSelectionChanged_Lambda([LandscapeBlock](FName Name, ESelectInfo::Type) {
			LandscapeBlock->SetText(FText::FromName(Name));
		})
		.OnGenerateWidget_Lambda([](FName Name){
			return SNew(STextBlock).Text(FText::FromName(Name));
		}) [
			LandscapeBlock
		];

	TSharedRef<SComboBox<TSharedPtr<FText>>> SourceSelector = SNew(SComboBox<TSharedPtr<FText>>)
		.OptionsSource(&RoadSources)
		.OnSelectionChanged_Lambda([SourceBlock](TSharedPtr<FText> SourceText, ESelectInfo::Type) {
			SourceBlock->SetText(*SourceText);
		})
		.OnGenerateWidget_Lambda([](TSharedPtr<FText> SourceText){
			return SNew(STextBlock).Text(*SourceText);
		}) [
			SourceBlock
		];

	TSharedRef<SButton> AddButton = SNew(SButton)
		.OnClicked_Lambda([=]()->FReply {
			FString LandscapeLabel = LandscapeBlock->GetText().ToString();
			FString LayerName = LayerNameBlock->GetText().ToString().TrimStartAndEnd();
			FText SourceText = SourceBlock->GetText();
			float RoadWidth = FCString::Atof(*RoadWidthBlock->GetText().ToString());

			FRoadBuilder* RoadBuilder = nullptr;
			if (SourceText.EqualTo(RoadLocalFileText))
			{
				FString XmlPath = XmlPathBlock->GetText().ToString().TrimStartAndEnd();
				RoadBuilder = new FRoadBuilder(HMTable, LandscapeLabel, LayerName, XmlPath, RoadWidth);
			}
			else if (SourceText.EqualTo(OverpassApiText))
			{
				FString OverpassQuery = OverpassBlock->GetText().ToString().TrimStartAndEnd();
				RoadBuilder = new FRoadBuilderOverpass(HMTable, LandscapeLabel, LayerName, RoadWidth, OverpassQuery);
			}
			else if (SourceText.EqualTo(OverpassShortQueryText))
			{
				FString OverpassShortQuery = OverpassShortBlock->GetText().ToString().TrimStartAndEnd();
				RoadBuilder = new FRoadBuilderShortQuery(HMTable, LandscapeLabel, LayerName, RoadWidth, OverpassShortQuery);
			}
			else if (SourceText.EqualTo(RoadSelectorText))
			{
				bool bMotorway = MotorwayCheckBox->IsChecked();
				bool bTrunk = TrunkCheckBox->IsChecked();
				bool bPrimary = PrimaryCheckBox->IsChecked();
				bool bSecondary = SecondaryCheckBox->IsChecked();
				bool bTertiary = TertiaryCheckBox->IsChecked();
				bool bUnclassified = UnclassifiedCheckBox->IsChecked();
				bool bResidential = ResidentialCheckBox->IsChecked();
				RoadBuilder = new FRoadBuilderSelector(HMTable, LandscapeLabel, LayerName, RoadWidth, bMotorway, bTrunk, bPrimary, bSecondary, bTertiary, bUnclassified, bResidential);
			}
			else if (SourceText.EqualTo(AllRoadsText))
			{
				RoadBuilder = new FRoadBuilderAll(HMTable, LandscapeLabel, LayerName, RoadWidth);
			}
			else check(false)

			if (RoadBuilder) AddRoadRow(RoadBuilder, true);

			return FReply::Handled();
		}) [
			SNew(SImage)
			.Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.Add"))
			.ToolTipText(LOCTEXT("AddRoadRow", "Add Road Row"))
		];
		
	Result->AddSlot().AutoWidth().Padding(FMargin(0, 0, 10, 0))[ LandscapeChoiceBlock ];
	Result->AddSlot().AutoWidth().Padding(FMargin(0, 0, 30, 0))[ LandscapeSelector ];
	Result->AddSlot().AutoWidth().Padding(FMargin(0, 0, 10, 0))[ SourceChoiceBlock ];
	Result->AddSlot().AutoWidth().Padding(FMargin(0, 0, 10, 0))[ SourceSelector ];
	Result->AddSlot().AutoWidth().Padding(FMargin(0, 0, 30, 0))[ RoadSelectorChoices ];
	Result->AddSlot().AutoWidth().Padding(FMargin(0, 0, 30, 0))[ XmlPathBlock ];
	Result->AddSlot().AutoWidth().Padding(FMargin(0, 0, 30, 0))[ OverpassBlock ];
	Result->AddSlot().AutoWidth().Padding(FMargin(0, 0, 30, 0))[ OverpassShortBlock ];
	//Result->AddSlot().AutoWidth().Padding(FMargin(0, 0, 20, 0))[ LayerNameBlock ];
	//Result->AddSlot().AutoWidth().Padding(FMargin(0, 0, 20, 0))[ RoadWidthBlock ];
	Result->AddSlot().AutoWidth().Padding(FMargin(0, 0, 0, 0))[ AddButton ];

	return Result;
}

void RoadTable::OnRemove(TSharedRef<SHorizontalBox> Row)
{
	RoadBuilders.Remove(RowToBuilder[Row]);
	RowToBuilder.Remove(Row);
}
