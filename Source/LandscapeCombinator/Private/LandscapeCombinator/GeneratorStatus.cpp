// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/GeneratorStatus.h"
#include "ConcurrencyHelpers/Concurrency.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

void UGeneratorStatus::NativeConstruct()
{
	Super::NativeConstruct();

	if (RowButton)
		RowButton->OnClicked.AddDynamic(this, &UGeneratorStatus::HandleRowClicked);
	if (GenerateButton) GenerateButton->OnClicked.AddDynamic(this, &UGeneratorStatus::HandleGenerateClicked);
	if (CancelButton) CancelButton->OnClicked.AddDynamic(this, &UGeneratorStatus::HandleCancelClicked);
	if (CleanButton) CleanButton->OnClicked.AddDynamic(this, &UGeneratorStatus::HandleCleanClicked);

	RefreshStatus();
}

void UGeneratorStatus::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

    Buttons.Tick(GenerateButton, SpinSpeed, InDeltaTime);

	TimeSinceLastRefresh += InDeltaTime;
	if (TimeSinceLastRefresh < RefreshInterval) return;

	TimeSinceLastRefresh = 0.0f;
	RefreshStatus();
}

void UGeneratorStatus::HandleRowClicked()
{
	AActor* Actor = TargetGenerator.Get();
	if (!IsValid(Actor)) return;

#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->SelectNone(/*bNoteSelectionChange=*/true, /*bDeselectBSPSurfs=*/true);
		GEditor->SelectActor(Actor, /*bInSelected=*/true, /*bNotify=*/true);
	}
#endif

	OnGeneratorClicked();
}

void UGeneratorStatus::RefreshStatus()
{
    if (!NameText || !StatusText || !TilesText) return;

    AActor* Actor = TargetGenerator.Get();
    if (!IsValid(Actor))
    {
        SetVisibility(ESlateVisibility::Collapsed);
        OnStatusRefreshed(0, 0, false);
        return;
    }

    SetVisibility(ESlateVisibility::Visible);

    int32 NumGenerated = 0;
    int32 NumPending = 0;
    bool bHasTileTracking = false;

    if (ULCPositionBasedGeneration* PBG = Actor->FindComponentByClass<ULCPositionBasedGeneration>())
    {
        if (PBG->bEnablePositionBasedGeneration)
        {
            NumGenerated = PBG->GeneratedTiles.Num();
            NumPending = PBG->PendingTiles.Num();
            bHasTileTracking = true;
        }
    }

    EGeneratorStatus GeneratorStatus = EGeneratorStatus::Idle;
    bool bIsGenerating = false;
    if (Actor->Implements<ULCGenerator>())
    {
        GeneratorStatus = Cast<ILCGenerator>(Actor)->GetGeneratorStatus();
        bIsGenerating = GeneratorStatus == EGeneratorStatus::Generating;
    }

    NameText->SetText(FText::FromString(Actor->GetActorNameOrLabel()));

    FString StatusString;
    switch (GeneratorStatus)
    {
        case EGeneratorStatus::Idle:       StatusString = TEXT("Idle"); break;
        case EGeneratorStatus::Generating: StatusString = TEXT("Generating..."); break;
        case EGeneratorStatus::Error:      StatusString = TEXT("Error"); break;
        case EGeneratorStatus::Success:    StatusString = TEXT("Success"); break;
        default:                           StatusString = TEXT("Unknown"); break;
    }
    StatusText->SetText(FText::FromString(StatusString));
    Buttons.Update(GenerateButton, CancelButton, CleanButton, true, bIsGenerating);

    if (bHasTileTracking)
    {
        TilesText->SetText(FText::FromString(
            FString::Printf(TEXT("%d/%d"), NumGenerated, NumGenerated + NumPending)));
        TilesText->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        TilesText->SetText(FText::GetEmpty());
        TilesText->SetVisibility(ESlateVisibility::Hidden);
    }

    UpdateRowColor(GeneratorStatus, NumGenerated, NumPending);

    OnStatusRefreshed(NumGenerated, NumPending, bIsGenerating);
}

void UGeneratorStatus::UpdateRowColor(EGeneratorStatus GeneratorStatus, int32 NumGenerated, int32 NumPending)
{
    if (!RowButton) return;

    FLinearColor Color;
    switch (GeneratorStatus)
    {
        case EGeneratorStatus::Error:
            Color = FLinearColor(0.8f, 0.1f, 0.1f); // red
            break;
        case EGeneratorStatus::Success:
            Color = FLinearColor(0.1f, 0.6f, 0.1f); // green
            break;
        case EGeneratorStatus::Generating:
            Color = FLinearColor(0.9f, 0.5f, 0.05f); // orange
            break;
        case EGeneratorStatus::Idle:
        default:
            Color = (NumPending > 0)
                ? FLinearColor(0.9f, 0.5f, 0.05f)   // orange: pending work queued while idle
                : FLinearColor(0.1f, 0.4f, 0.8f);   // blue: idle / nothing generated
            break;
    }

    RowButton->SetBackgroundColor(Color);
}

void UGeneratorStatus::HandleGenerateClicked()
{
	AActor* Actor = TargetGenerator.Get();
	if (!IsValid(Actor)) return;

	ILCGenerator* Generator = Cast<ILCGenerator>(Actor);
	if (!Generator || Generator->GetGeneratorStatus() == EGeneratorStatus::Generating) return;

	Generator->GenerateFromGameThread(FName(), true);
}

void UGeneratorStatus::HandleCancelClicked()
{
	AActor* Actor = TargetGenerator.Get();
	if (IsValid(Actor) && Actor->Implements<ULCGenerator>())
	{
		Concurrency::SetCancelRequested(true);
	}
}

void UGeneratorStatus::HandleCleanClicked()
{
	AActor* Actor = TargetGenerator.Get();
	if (!IsValid(Actor) || !Actor->Implements<ULCGenerator>()) return;

	ILCGenerator* Generator = Cast<ILCGenerator>(Actor);
	if (Generator && Generator->GetGeneratorStatus() == EGeneratorStatus::Generating) return;

	ILCGenerator::Execute_Cleanup(Actor, false);
}
