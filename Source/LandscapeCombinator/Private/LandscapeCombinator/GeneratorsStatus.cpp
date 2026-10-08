// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/GeneratorsStatus.h"
#include "LandscapeCombinator/LandscapeCombination.h"
#include "LCCommon/LCGenerator.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetTree.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

namespace
{
	struct FGeneratorTabData
	{
		FString Label;
		TWeakObjectPtr<AActor> TabActor; // the ALandscapeCombination for this tab, null for "Other"
		TArray<TWeakObjectPtr<AActor>> Generators;
	};
}

void UGeneratorsStatus::NativeConstruct()
{
	Super::NativeConstruct();

if (PanelBorder) PanelBorder->SetBrushColor(ActiveTabColor);

	if (GenerateAllButton) GenerateAllButton->OnClicked.AddDynamic(this, &UGeneratorsStatus::HandleGenerateAllClicked);
	if (CancelAllButton) CancelAllButton->OnClicked.AddDynamic(this, &UGeneratorsStatus::HandleCancelAllClicked);
	if (CleanAllButton) CleanAllButton->OnClicked.AddDynamic(this, &UGeneratorsStatus::HandleCleanAllClicked);

	RefreshGeneratorsList();
}

bool UGeneratorsStatus::IsGeneratorExcluded(AActor* Generator) const
{
	if (!IsValid(Generator)) return true;

	for (const TSubclassOf<AActor>& ExcludedClass : ExcludedGeneratorClasses)
	{
		if (*ExcludedClass && Generator->IsA(ExcludedClass))
		{
			return true;
		}
	}
	return false;
}

void UGeneratorsStatus::RefreshGeneratorsList()
{
	if (!IsValid(TabSwitcher) || !IsValid(TabButtonBox)) return;
	if (!GeneratorStatusClass || !TabButtonClass) return;

	UWorld* TargetWorld = nullptr;
#if WITH_EDITOR
	if (GEditor && GEditor->PlayWorld) TargetWorld = GEditor->PlayWorld;
	else if (GEditor) TargetWorld = GEditor->GetEditorWorldContext().World();
#endif
	if (!TargetWorld) TargetWorld = GetWorld();
	if (!TargetWorld) return;

	TabSwitcher->ClearChildren();
	TabButtonBox->ClearChildren();
	TabButtons.Empty();
	TabInfos.Reset();

	TArray<AActor*> AllCombinationActors;
	UGameplayStatics::GetAllActorsOfClass(TargetWorld, ALandscapeCombination::StaticClass(), AllCombinationActors);

	TArray<ALandscapeCombination*> Combinations;
	for (AActor* Actor : AllCombinationActors)
	{
		if (ALandscapeCombination* Combination = Cast<ALandscapeCombination>(Actor))
		{
			Combinations.Add(Combination);
		}
	}
	Combinations.Sort([](const ALandscapeCombination& A, const ALandscapeCombination& B) {
		return A.GetActorNameOrLabel() < B.GetActorNameOrLabel();
	});

	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(TargetWorld, AActor::StaticClass(), AllActors);

	TSet<AActor*> AllGenerators;
	for (AActor* Actor : AllActors)
	{
		if (IsValid(Actor) && Actor->Implements<ULCGenerator>() && !IsGeneratorExcluded(Actor))
		{
			AllGenerators.Add(Actor);
		}
	}

	TSet<AActor*> UsedGenerators;
	TArray<FGeneratorTabData> Tabs;

	// One tab per combination=
	for (ALandscapeCombination* Combination : Combinations)
	{
		FGeneratorTabData TabData;
		TabData.Label = Combination->GetActorNameOrLabel();
		TabData.TabActor = Combination;

        for (const FGeneratorWrapper& Wrapper : Combination->Generators)
        {
            if (!Wrapper.Generator.IsValid()) continue;
            if (!Wrapper.Generator->Implements<ULCGenerator>()) continue;

            AActor* GeneratorActor = Wrapper.Generator.Get();
            if (IsGeneratorExcluded(GeneratorActor)) continue;

            UsedGenerators.Add(GeneratorActor);
            if (!Wrapper.bIsEnabled) continue;

            TabData.Generators.Add(GeneratorActor);
        }

		if (TabData.Generators.Num() > 0)
		{
			Tabs.Add(MoveTemp(TabData));
		}
	}

	// Extra tab for generators not referenced by a combination
	{
		FGeneratorTabData OtherTab;
		OtherTab.Label = OtherGeneratorsTabLabel;

		TArray<AActor*> OtherActors;
		for (AActor* Generator : AllGenerators)
		{
			if (!UsedGenerators.Contains(Generator))
			{
				OtherActors.Add(Generator);
			}
		}
		OtherActors.Sort([](const AActor& A, const AActor& B) {
			return A.GetActorNameOrLabel() < B.GetActorNameOrLabel();
		});

		for (AActor* Generator : OtherActors)
		{
			OtherTab.Generators.Add(Generator);
		}

		if (OtherTab.Generators.Num() > 0)
		{
			Tabs.Add(MoveTemp(OtherTab));
		}
	}

	// switcher page + tab button per non-empty tab
	for (int32 TabIndex = 0; TabIndex < Tabs.Num(); ++TabIndex)
	{
		const FGeneratorTabData& TabData = Tabs[TabIndex];

		FGeneratorTabInfo& TabInfo = TabInfos.AddDefaulted_GetRef();
		TabInfo.TabActor = TabData.TabActor;

		UVerticalBox* TabContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		for (const TWeakObjectPtr<AActor>& GeneratorPtr : TabData.Generators)
		{
			AActor* Generator = GeneratorPtr.Get();
			if (!IsValid(Generator)) continue;

			UGeneratorStatus* Row = CreateWidget<UGeneratorStatus>(this, GeneratorStatusClass);
			if (!IsValid(Row)) continue;

			Row->TargetGenerator = Generator;
			TabContent->AddChildToVerticalBox(Row);
		}
		TabSwitcher->AddChild(TabContent);

		UGeneratorsStatusTabButton* TabButton = CreateWidget<UGeneratorsStatusTabButton>(this, TabButtonClass);
		if (!IsValid(TabButton)) continue;

		TabButton->InitializeTab(this, TabIndex, FText::FromString(TabData.Label), TabData.TabActor.Get());
		TabButtons.Add(TabButton);
		TabButtonBox->AddChildToHorizontalBox(TabButton);
	}

	if (Tabs.Num() > 0) SetActiveTab(0);
}

void UGeneratorsStatus::SetActiveTab(int32 TabIndex)
{
	if (!IsValid(TabSwitcher)) return;
	if (TabSwitcher->GetChildrenCount() == 0) return;

	TabIndex = FMath::Clamp(TabIndex, 0, TabSwitcher->GetChildrenCount() - 1);
	TabSwitcher->SetActiveWidgetIndex(TabIndex);
	ActiveTabIndex = TabIndex;
	UpdateAllButtons();

	for (int32 i = 0; i < TabButtons.Num(); ++i)
	{
		if (UGeneratorsStatusTabButton* Button = TabButtons[i])
		{
			Button->SetActive(i == TabIndex);
		}
	}

	OnTabChanged(TabIndex);
}

void UGeneratorsStatus::HandleGenerateAllClicked() { GenerateAll(); }
void UGeneratorsStatus::HandleCancelAllClicked() { CancelAll(); }
void UGeneratorsStatus::HandleCleanAllClicked() { CleanAll(); }

ALandscapeCombination* UGeneratorsStatus::GetActiveCombination() const
{
	return TabInfos.IsValidIndex(ActiveTabIndex) ? Cast<ALandscapeCombination>(TabInfos[ActiveTabIndex].TabActor.Get()) : nullptr;
}

void UGeneratorsStatus::GenerateAll()
{
	ALandscapeCombination* Combination = GetActiveCombination();
	if (Combination && Combination->GetGeneratorStatus() != EGeneratorStatus::Generating) Combination->GenerateActors();
}

void UGeneratorsStatus::CancelAll()
{
	if (ALandscapeCombination* Combination = GetActiveCombination()) Combination->CancelGeneration();
}

void UGeneratorsStatus::CleanAll()
{
	ALandscapeCombination* Combination = GetActiveCombination();
	if (Combination && Combination->GetGeneratorStatus() != EGeneratorStatus::Generating) Combination->DeleteActors();
}


void UGeneratorsStatus::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    UpdateAllButtons();
    Buttons.Tick(GenerateAllButton, SpinSpeed, InDeltaTime);
}

void UGeneratorsStatus::UpdateAllButtons()
{
    ALandscapeCombination* Combination = GetActiveCombination();
    const bool bGenerating = Combination && Combination->GetGeneratorStatus() == EGeneratorStatus::Generating;
    Buttons.Update(GenerateAllButton, CancelAllButton, CleanAllButton, Combination != nullptr, bGenerating);
}
