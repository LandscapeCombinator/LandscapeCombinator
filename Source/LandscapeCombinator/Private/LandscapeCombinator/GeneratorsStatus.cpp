// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/GeneratorsStatus.h"
#include "LandscapeCombinator/LandscapeCombination.h"
#include "LCCommon/LCGenerator.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetTree.h"
#include "EngineUtils.h"

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
	if (TabDropdown) TabDropdown->OnSelectionChanged.AddDynamic(this, &UGeneratorsStatus::HandleTabDropdownSelection);

	if (AddGeneratorDropdown)
	{
		AddGeneratorDropdown->ClearOptions();
		for (TSubclassOf<AActor> GenClass : GeneratorClasses)
		{
			if (IsValid(GenClass))
				AddGeneratorDropdown->AddOption(GenClass->GetName());
		}
		AddGeneratorDropdown->OnSelectionChanged.AddDynamic(this, &UGeneratorsStatus::HandleAddGeneratorSelection);
	}

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
	if (!IsValid(TabSwitcher)) return;
	if (!GeneratorStatusClass) return;

	UWorld* TargetWorld = GetTargetWorld();
	if (!TargetWorld) return;

	const int32 PreviousTab = ActiveTabIndex;
	bDirty = false;
	ClearTabs();

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

	// One tab per combination
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

		Tabs.Add(MoveTemp(TabData));
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

		if (TabDropdown) TabDropdown->AddOption(TabData.Label);
	}

	if (TabDropdown) TabDropdown->SetVisibility(Tabs.Num() > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (Tabs.Num() > 0) SetActiveTab(PreviousTab);
	LastSignature = ComputeSignature();
}

void UGeneratorsStatus::SetActiveTab(int32 TabIndex)
{
	if (!IsValid(TabSwitcher)) return;
	if (TabSwitcher->GetChildrenCount() == 0) return;

	TabIndex = FMath::Clamp(TabIndex, 0, TabSwitcher->GetChildrenCount() - 1);
	TabSwitcher->SetActiveWidgetIndex(TabIndex);
	ActiveTabIndex = TabIndex;
	if (TabDropdown && TabDropdown->GetSelectedIndex() != TabIndex) TabDropdown->SetSelectedIndex(TabIndex);
		UpdateAllButtons();

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
	TimeSinceRefresh += InDeltaTime;
	if (bDirty || TimeSinceRefresh >= 0.25f)
	{
		TimeSinceRefresh = 0.f;
		if (bDirty || ComputeSignature() != LastSignature) RefreshGeneratorsList();
	}
	UpdateAllButtons();
	Buttons.Tick(GenerateAllButton, SpinSpeed, InDeltaTime);
}

void UGeneratorsStatus::UpdateAllButtons()
{
	ALandscapeCombination* Combination = GetActiveCombination();
	const bool bGenerating = Combination && Combination->GetGeneratorStatus() == EGeneratorStatus::Generating;
	Buttons.Update(GenerateAllButton, CancelAllButton, CleanAllButton, Combination != nullptr, bGenerating);
}

void UGeneratorsStatus::HandleAddGeneratorSelection(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (SelectedItem.IsEmpty() || SelectionType == ESelectInfo::OnNavigation) return;

	UClass* SelectedClass = nullptr;
	for (TSubclassOf<AActor> GenClass : GeneratorClasses)
	{
		if (IsValid(GenClass))
		{
			if (SelectedItem == GenClass->GetName())
			{
				SelectedClass = *GenClass;
				break;
			}
		}
	}

	if (!IsValid(SelectedClass)) return;

	UWorld* TargetWorld = GetTargetWorld();
	if (!TargetWorld) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;

	ALandscapeCombination* LandscapeCombination = GetActiveCombination();
	if (IsValid(LandscapeCombination))
	{
		SpawnLocation = LandscapeCombination->GetActorLocation();
	}

	AActor* NewActor = TargetWorld->SpawnActor<AActor>(SelectedClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (IsValid(NewActor))
	{
		if (IsValid(LandscapeCombination))
		{
			FGeneratorWrapper NewWrapper;
			NewWrapper.Generator = NewActor;
			NewWrapper.bIsEnabled = true;
			LandscapeCombination->Generators.Add(NewWrapper);
			LandscapeCombination->Modify();
		}

		RefreshGeneratorsList();
	}

	AddGeneratorDropdown->ClearSelection();
}

UWorld* UGeneratorsStatus::GetTargetWorld() const
{
	UWorld* World = nullptr;
#if WITH_EDITOR
	if (GEditor) World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
#endif
	return World ? World : GetWorld();
}

void UGeneratorsStatus::ClearTabs()
{
	if (TabSwitcher) TabSwitcher->ClearChildren();
	if (TabDropdown) TabDropdown->ClearOptions();
	TabInfos.Reset();
}

void UGeneratorsStatus::InvalidateGeneratorsList()
{
	ClearTabs();
	bDirty = true;
}

uint32 UGeneratorsStatus::ComputeSignature() const
{
	uint32 Hash = 0;
	UWorld* World = GetTargetWorld();
	if (!World) return Hash;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor)) continue;

		if (const ALandscapeCombination* Combination = Cast<ALandscapeCombination>(Actor))
		{
			Hash = HashCombine(Hash, GetTypeHash(Actor));
			Hash = HashCombine(Hash, GetTypeHash(Actor->GetActorNameOrLabel()));
			for (const FGeneratorWrapper& Wrapper : Combination->Generators)
			{
				Hash = HashCombine(Hash, GetTypeHash(Wrapper.Generator.Get()));
				Hash = HashCombine(Hash, GetTypeHash((int32) Wrapper.bIsEnabled));
			}
		}
		else if (Actor->Implements<ULCGenerator>() && !IsGeneratorExcluded(Actor))
		{
			Hash = HashCombine(Hash, GetTypeHash(Actor));
		}
	}
	return Hash;
}

void UGeneratorsStatus::HandleTabDropdownSelection(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	if (!TabDropdown || SelectionType == ESelectInfo::Direct) return;
	const int32 Index = TabDropdown->FindOptionIndex(SelectedItem);
	if (Index != INDEX_NONE) SetActiveTab(Index);
}
