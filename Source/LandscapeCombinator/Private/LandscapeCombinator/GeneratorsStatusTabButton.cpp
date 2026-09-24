// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/GeneratorsStatusTabButton.h"
#include "LandscapeCombinator/GeneratorsStatus.h"


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
	OnActiveChanged(bIsActive);
}
