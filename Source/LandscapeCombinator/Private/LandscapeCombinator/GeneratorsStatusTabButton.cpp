// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/GeneratorsStatusTabButton.h"
#include "LandscapeCombinator/GeneratorsStatus.h"
#include "Brushes/SlateColorBrush.h"


void UGeneratorsStatusTabButton::InitializeTab(UGeneratorsStatus* InOwner, int32 InTabIndex, const FText& InLabel, AActor* InTargetActor)
{
	Owner = InOwner;
	TabIndex = InTabIndex;
	TargetActor = InTargetActor;

	if (ButtonRoot)
		ButtonRoot->OnClicked.AddDynamic(this, &UGeneratorsStatusTabButton::HandleClicked);

	if (LabelText)
		LabelText->SetText(InLabel);

	OnLabelSet(InLabel);
	ApplyColor();
}

void UGeneratorsStatusTabButton::HandleClicked()
{
	if (UGeneratorsStatus* OwnerPtr = Owner.Get())
	{
		OwnerPtr->SetActiveTab(TabIndex);
	}

#if WITH_EDITOR
	AActor* Actor = TargetActor.Get();
	if (IsValid(Actor) && GEditor)
	{
		GEditor->SelectNone(/*bNoteSelectionChange=*/true, /*bDeselectBSPSurfs=*/true);
		GEditor->SelectActor(Actor, /*bInSelected=*/true, /*bNotify=*/true);
	}
#endif
}

void UGeneratorsStatusTabButton::SetActive(bool bNewActive)
{
	if (bIsActive == bNewActive) return;
	bIsActive = bNewActive;
	ApplyColor();
	OnActiveChanged(bIsActive);
}


void UGeneratorsStatusTabButton::ApplyColor()
{
	if (!ButtonRoot) return;

	FButtonStyle Style = ButtonRoot->GetStyle();
	Style.SetNormal(FSlateColorBrush(FLinearColor::White));
	Style.SetHovered(FSlateColorBrush(FLinearColor(0.85f, 0.85f, 0.85f)));
	Style.SetPressed(FSlateColorBrush(FLinearColor(0.7f, 0.7f, 0.7f)));
	ButtonRoot->SetStyle(Style);
	if (const UGeneratorsStatus* OwnerPtr = Owner.Get())
		ButtonRoot->SetBackgroundColor(bIsActive ? OwnerPtr->ActiveTabColor : OwnerPtr->InactiveTabColor);
}
