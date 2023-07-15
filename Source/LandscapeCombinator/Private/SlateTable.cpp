// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#include "SlateTable.h"
#include "LandscapeCombinatorStyle.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

SlateTable::SlateTable()
{
	SAssignNew(Rows, SVerticalBox);
}

void SlateTable::OnRemove(TSharedRef<SHorizontalBox> Line)
{
}

int32 SlateTable::GetIndex(TSharedRef<SHorizontalBox> Row)
{
	for (int i = 0; i < Rows->NumSlots(); i++)
	{
		if (Rows->GetSlot(i).GetWidget() == Row)
		{
			return i;
		}
	}
	return 0;
}


void SlateTable::AddRow(TSharedRef<SHorizontalBox> Row, bool bControlButtons, bool bSave)
{

	if (bControlButtons)
	{
		TSharedRef<SButton> RemoveButton = SNew(SButton)
			.OnClicked_Lambda([this, Row]()->FReply {
				OnRemove(Row);
				Rows->RemoveSlot(Row);
				Save();
				return FReply::Handled();
			}) [
				SNew(SImage).Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.Delete"))
				.ToolTipText(LOCTEXT("RemoveRow", "Remove Heightmap Row"))
			];
		
		TSharedRef<SButton> UpButton = SNew(SButton)
			.OnClicked_Lambda([this, Row]()->FReply {
				int32 i = GetIndex(Row);
				Rows->RemoveSlot(Row);
				Rows->InsertSlot(FMath::Max(0, i - 1)).AutoHeight().Padding(FMargin(0, 0, 0, 8))[ Row ];
				Save();
				return FReply::Handled();
			}) [
				SNew(SImage).Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.MoveUp"))
				.ToolTipText(LOCTEXT("MoveDown", "Move Up"))
			];

		TSharedRef<SButton> DownButton = 
			SNew(SButton)
			.OnClicked_Lambda([this, Row]()->FReply {
				int32 i = GetIndex(Row);
				Rows->RemoveSlot(Row);
				Rows->InsertSlot(FMath::Min(Rows->NumSlots(), i + 1)).AutoHeight().Padding(FMargin(0, 0, 0, 8))[ Row ];
				Save();
				return FReply::Handled();
			}) [
				SNew(SImage)
				.ColorAndOpacity(FColor(100, 0, 0))
				.Image(FLandscapeCombinatorStyle::Get().GetBrush("LandscapeCombinator.MoveDown"))
				.ToolTipText(LOCTEXT("MoveDown", "Move Down"))
			];

		TSharedRef<SHorizontalBox> ControlButtons = SNew(SHorizontalBox)
			+SHorizontalBox::Slot().AutoWidth()[ ButtonBox(RemoveButton) ]
			+SHorizontalBox::Slot().AutoWidth()[ ButtonBox(DownButton) ]
			+SHorizontalBox::Slot().AutoWidth()[ ButtonBox(UpButton) ];

		Row->AddSlot().FillWidth(1)[ ControlButtons ];
	}

	Rows->AddSlot().AutoHeight().Padding(FMargin(0, 0, 0, 8))[ Row ];

	if (bSave) Save();
}

TSharedRef<SBox> SlateTable::ButtonBox(TSharedRef<SButton> Button)
{
	const FButtonStyle* ButtonStyle = &FLandscapeCombinatorStyle::Get().GetWidgetStyle<FButtonStyle>("LandscapeCombinator.Button");
	Button->SetButtonStyle(ButtonStyle);
	return SNew(SBox)
			.Padding(0)
			.WidthOverride(ButtonWidth)
			[ Button ];
}

TSharedRef<SWidget> SlateTable::MakeTable()
{
	Load();

	return SNew(SVerticalBox)
	+SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 15)) [ Header() ]
	+SVerticalBox::Slot().AutoHeight().Padding(FMargin(0, 0, 0, 15)) [ Rows.ToSharedRef() ]
	+SVerticalBox::Slot().AutoHeight() [ Footer() ]
	;
}

EVisibility SlateTable::VisibleFrom(int Threshold) const
{
	if (Rows->GetPaintSpaceGeometry().GetAbsoluteSize().X > Threshold)
	{
		return EVisibility::Visible;
	}
	else
	{
		return EVisibility::Collapsed;
	}
}

#undef LOCTEXT_NAMESPACE