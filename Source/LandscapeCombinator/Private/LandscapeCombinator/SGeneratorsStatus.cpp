// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#include "LandscapeCombinator/SGeneratorsStatus.h"
#include "LandscapeCombinator/LandscapeCombination.h"
#include "LandscapeCombinator/LCIcons.h"
#include "Coordinates/LevelCoordinates.h"
#include "BuildingsFromSplines/Building.h"
#include "LCCommon/LCGenerator.h"
#include "ConcurrencyHelpers/Concurrency.h"

#include "Widgets/Images/SImage.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "Input/DragAndDrop.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/CoreStyle.h"
#include "Misc/ConfigCacheIni.h"
#include "EngineUtils.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Builders/CubeBuilder.h"
#include "GameFramework/Volume.h"
#include "ActorFactories/ActorFactory.h"
#endif

DECLARE_DELEGATE_TwoParams(FOnMoveGenerator, int32, int32);

namespace
{
	constexpr double SpinDegreesPerSecond = 360.0;

	const TCHAR* ScaleSection = TEXT("LandscapeCombinator");
	const TCHAR* ScaleKey = TEXT("UIScale");
	float SavedScale = 0.f;

	const FString& ScaleIni()
	{
#if WITH_EDITOR
		return GEditorPerProjectIni;
#else
		return GGameUserSettingsIni;
#endif
	}

	TArray<UClass*> GetGeneratorClasses()
	{
		static const TCHAR* Paths[] = {
			TEXT("/Script/LandscapeCombinator.LandscapeSpawner"),
			TEXT("/Script/LandscapeCombinator.LandscapeMeshSpawner"),
			TEXT("/Script/LandscapeCombinator.LandscapeTexturer"),
			TEXT("/Script/SplineImporter.SplineImporter"),
			TEXT("/Script/BuildingsFromSplines.RoadsFromSplines"),
			TEXT("/Script/BuildingsFromSplines.BuildingsFromSplines"),
			TEXT("/Script/SplineImporter.OGRGeometry"),
			TEXT("/Script/LandscapeCombinator.LandscapePCGVolume"),
			TEXT("/Script/LandscapeCombinator.LandscapeCombination")
		};

		TArray<UClass*> Classes;
		for (const TCHAR* Path : Paths)
		{
			if (UClass* Class = LoadClass<AActor>(nullptr, Path)) Classes.Add(Class);
		}
		return Classes;
	}

	bool IsExcluded(const AActor* Actor)
	{
		return !IsValid(Actor) || Actor->IsA<ABuilding>() || Actor->IsA<ALandscapeCombination>();
	}

	ILCGenerator* AsGenerator(AActor* Actor)
	{
		return IsValid(Actor) && Actor->Implements<ULCGenerator>() ? Cast<ILCGenerator>(Actor) : nullptr;
	}

	EGeneratorStatus GetStatus(AActor* Actor)
	{
		ILCGenerator* Generator = AsGenerator(Actor);
		return Generator ? Generator->GetGeneratorStatus() : EGeneratorStatus::Idle;
	}

	FString StatusString(AActor* Actor)
	{
		switch (GetStatus(Actor))
		{
			case EGeneratorStatus::Idle: return TEXT("Idle");
			case EGeneratorStatus::Generating: return TEXT("Generating...");
			case EGeneratorStatus::Error: return TEXT("Error");
			case EGeneratorStatus::Success: return TEXT("Success");
			default: return TEXT("Unknown");
		}
	}

	bool GetTiles(AActor* Actor, int32& NumGenerated, int32& NumPending)
	{
		if (!IsValid(Actor)) return false;

		ULCPositionBasedGeneration* PBG = Actor->FindComponentByClass<ULCPositionBasedGeneration>();
		if (!PBG || !PBG->bEnablePositionBasedGeneration) return false;

		NumGenerated = PBG->GeneratedTiles.Num();
		NumPending = GetStatus(Actor) == EGeneratorStatus::Generating ? PBG->PendingTiles.Num() : 0;
		return true;
	}

	const FButtonStyle& RowButtonStyle()
	{
		static const FButtonStyle Style = FButtonStyle()
			.SetNormal(FSlateRoundedBoxBrush(FLinearColor(0.85f, 0.85f, 0.85f), 4.f))
			.SetHovered(FSlateRoundedBoxBrush(FLinearColor::White, 4.f))
			.SetPressed(FSlateRoundedBoxBrush(FLinearColor(0.65f, 0.65f, 0.65f), 4.f))
			.SetNormalPadding(FMargin(0.f))
			.SetPressedPadding(FMargin(0.f));
		return Style;
	}

	FLinearColor RowColor(AActor* Actor, bool bEnabled = true)
	{
		if (!bEnabled) return FLinearColor::FromSRGBColor(FColor(90, 90, 90));

		int32 NumGenerated = 0, NumPending = 0;
		GetTiles(Actor, NumGenerated, NumPending);

		switch (GetStatus(Actor))
		{
			case EGeneratorStatus::Error: return FLinearColor::FromSRGBColor(FColor(190, 50, 50));
			case EGeneratorStatus::Success: return FLinearColor::FromSRGBColor(FColor(60, 150, 60));
			case EGeneratorStatus::Generating: return FLinearColor::FromSRGBColor(FColor(215, 125, 30));
			default: break;
		}
		return NumPending > 0 ? FLinearColor::FromSRGBColor(FColor(215, 125, 30)) : FLinearColor::FromSRGBColor(FColor(60, 120, 170));
	}

	void Generate(AActor* Actor)
	{
		ILCGenerator* Generator = AsGenerator(Actor);
		if (!Generator || Generator->GetGeneratorStatus() == EGeneratorStatus::Generating) return;

		if (ALandscapeCombination* Combination = Cast<ALandscapeCombination>(Actor)) Combination->GenerateActors();
		else Generator->GenerateFromGameThread(FName(), true);
	}

	void Cancel(AActor* Actor)
	{
		if (AsGenerator(Actor)) Concurrency::SetCancelRequested(true);
	}

	void Clean(AActor* Actor)
	{
		ILCGenerator* Generator = AsGenerator(Actor);
		if (!Generator || Generator->GetGeneratorStatus() == EGeneratorStatus::Generating) return;

		ILCGenerator::Execute_Cleanup(Actor, false);
	}

	TSharedRef<SWidget> MakeIconButton(
		const FSlateBrush* Icon, const FText& Tooltip,
		TFunction<EVisibility()> GetVisibility, TFunction<bool()> IsSpinning, TFunction<void()> OnClicked)
	{
		return SNew(SButton)
		.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
		.ContentPadding(FMargin(1.f))
		.ToolTipText(Tooltip)
		.Visibility(TAttribute<EVisibility>::CreateLambda(GetVisibility))
		.OnClicked_Lambda([OnClicked]()
		{
			OnClicked();
			return FReply::Handled();
		})
		[
			SNew(SBox).WidthOverride(16.f).HeightOverride(16.f)
			[
				SNew(SImage)
				.Image(Icon)
				.RenderTransformPivot(FVector2D(0.5, 0.5))
				.RenderTransform_Lambda([IsSpinning]() -> TOptional<FSlateRenderTransform>
				{
					if (!IsSpinning()) return TOptional<FSlateRenderTransform>();
					const double Degrees = FMath::Fmod(FPlatformTime::Seconds() * SpinDegreesPerSecond, 360.0);
					return FSlateRenderTransform(FQuat2D(FMath::DegreesToRadians(Degrees)));
				})
			]
		];
	}

	TSharedRef<SWidget> MakeControls(TFunction<AActor*()> GetTarget)
	{
		auto Available = [GetTarget]() { return AsGenerator(GetTarget()) != nullptr; };
		auto Busy = [GetTarget]() { return GetStatus(GetTarget()) == EGeneratorStatus::Generating; };

		return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth()
		[
			MakeIconButton(LCIcons::Refresh(), INVTEXT("Generate"),
				[Available, Busy]() { return !Available() ? EVisibility::Collapsed : (Busy() ? EVisibility::HitTestInvisible : EVisibility::Visible); },
				Busy,
				[GetTarget]() { Generate(GetTarget()); })
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			MakeIconButton(LCIcons::Cancel(), INVTEXT("Cancel"),
				[Available, Busy]() { return Available() && Busy() ? EVisibility::Visible : EVisibility::Collapsed; },
				[]() { return false; },
				[GetTarget]() { Cancel(GetTarget()); })
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			MakeIconButton(LCIcons::Trash(), INVTEXT("Clean"),
				[Available, Busy]() { return Available() && !Busy() ? EVisibility::Visible : EVisibility::Collapsed; },
				[]() { return false; },
				[GetTarget]() { Clean(GetTarget()); })
		];
	}

	class FGeneratorDragOp : public FDragDropOperation
	{
	public:
		DRAG_DROP_OPERATOR_TYPE(FGeneratorDragOp, FDragDropOperation)

		FGeneratorDragOp(int32 InFrom, const FText& InLabel) : From(InFrom), Label(InLabel) {}

		virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
		{
			return SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
			.Padding(6.f)
			[
				SNew(STextBlock).Text(Label)
			];
		}

		int32 From;
		FText Label;
	};

	class SGeneratorRow : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SGeneratorRow) {}
			SLATE_ARGUMENT(int32, Index)
			SLATE_ARGUMENT(FText, Label)
			SLATE_EVENT(FOnMoveGenerator, OnMove)
			SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Index = InArgs._Index;
			Label = InArgs._Label;
			OnMove = InArgs._OnMove;

			ChildSlot
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
				.BorderBackgroundColor_Lambda([this]() -> FSlateColor { return bDragOver ? FLinearColor::White : FLinearColor::Transparent; })
				.Padding(1.f)
				[
					InArgs._Content.Widget
				]
			];
		}

		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			if (Index == INDEX_NONE || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
			return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
		}

		virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
		{
			return FReply::Handled().BeginDragDrop(MakeShared<FGeneratorDragOp>(Index, Label));
		}

		virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
		{
			bDragOver = Index != INDEX_NONE && DragDropEvent.GetOperationAs<FGeneratorDragOp>().IsValid();
		}

		virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override
		{
			bDragOver = false;
		}

		virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
		{
			return bDragOver ? FReply::Handled() : FReply::Unhandled();
		}

		virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
		{
			bDragOver = false;
			TSharedPtr<FGeneratorDragOp> Op = DragDropEvent.GetOperationAs<FGeneratorDragOp>();
			if (!Op.IsValid() || Index == INDEX_NONE) return FReply::Unhandled();

			OnMove.ExecuteIfBound(Op->From, Index);
			return FReply::Handled();
		}

	private:
		int32 Index = INDEX_NONE;
		FText Label;
		FOnMoveGenerator OnMove;
		bool bDragOver = false;
	};
}

void SGeneratorsStatus::Construct(const FArguments& InArgs)
{
	RuntimeWorld = InArgs._World;
	OnGeneratorPicked = InArgs._OnGeneratorPicked;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
		.BorderBackgroundColor(FLinearColor::FromSRGBColor(FColor(62, 62, 72)))
		.Padding(4.f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SAssignNew(TabCombo, SComboBox<TSharedPtr<FString>>)
					.OptionsSource(&TabLabels)
					.Visibility_Lambda([this]() { return Tabs.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
					.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) -> TSharedRef<SWidget>
					{
						return SNew(STextBlock).Text(FText::FromString(*Item));
					})
					.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo)
					{
						if (!Item.IsValid() || SelectInfo == ESelectInfo::Direct) return;
						SelectTab(TabLabels.IndexOfByKey(Item));
					})
					[
						SNew(STextBlock)
						.Text_Lambda([this]() -> FText
						{
							return TabLabels.IsValidIndex(ActiveTab) ? FText::FromString(*TabLabels[ActiveTab]) : FText::GetEmpty();
						})
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 0, 0)
				[
					MakeIconButton(LCIcons::Settings(), INVTEXT("Show the settings of this combination"),
						[this]() { return GetActiveCombination() ? EVisibility::Visible : EVisibility::Collapsed; },
						[]() { return false; },
						[this]() { PickActor(GetActiveCombination()); })
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					MakeControls([this]() -> AActor* { return GetActiveCombination(); })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 4, 0)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					.Text(INVTEXT("Add Generator"))
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SComboButton)
					.HasDownArrow(false)
					.OnGetMenuContent(this, &SGeneratorsStatus::MakeAddMenu)
					.ButtonContent()
					[
						SNew(STextBlock).Text(INVTEXT("+"))
					]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
			[
				MakeLevelCoordinatesRow()
			]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(RowsBox, SVerticalBox)
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)
			[
				SNew(SExpandableArea)
				.InitiallyCollapsed(true)
				.AreaTitle(INVTEXT("Options"))
				.BodyContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text(INVTEXT("UI scale"))
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(6, 0)
					[
						SNew(SSlider)
						.MinValue(0.5f)
						.MaxValue(2.f)
						.StepSize(0.05f)
						.Value_Lambda([this]() { return bDraggingScale ? PendingScale : GetUIScale(); })
						.OnMouseCaptureBegin_Lambda([this]() { bDraggingScale = true; PendingScale = GetUIScale(); })
						.OnValueChanged_Lambda([this](float Value) { PendingScale = Value; })
						.OnMouseCaptureEnd_Lambda([this]() { bDraggingScale = false; SetUIScale(PendingScale); })
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt((bDraggingScale ? PendingScale : GetUIScale()) * 100.f))); })
					]
				]
			]
		]
	];

	Rebuild();
	RegisterActiveTimer(0.25f, FWidgetActiveTimerDelegate::CreateSP(this, &SGeneratorsStatus::OnRefreshTimer));
}

EActiveTimerReturnType SGeneratorsStatus::OnRefreshTimer(double InCurrentTime, float InDeltaTime)
{
	if (ComputeSignature() != LastSignature) Rebuild();
	return EActiveTimerReturnType::Continue;
}

UWorld* SGeneratorsStatus::GetTargetWorld() const
{
#if WITH_EDITOR
	if (GEditor)
	{
		UWorld* World = GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World();
		if (World) return World;
	}
#endif
	return RuntimeWorld.Get();
}

ALandscapeCombination* SGeneratorsStatus::GetActiveCombination() const
{
	return Tabs.IsValidIndex(ActiveTab) ? Cast<ALandscapeCombination>(Tabs[ActiveTab].TabActor.Get()) : nullptr;
}

uint32 SGeneratorsStatus::ComputeSignature() const
{
	uint32 Hash = 0;
	UWorld* World = GetTargetWorld();
	if (!World) return Hash;

	Hash = HashCombine(Hash, GetTypeHash(World));
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor)) continue;

		if (Actor->IsA<ALevelCoordinates>()) Hash = HashCombine(Hash, GetTypeHash(Actor));
		else if (const ALandscapeCombination* Combination = Cast<ALandscapeCombination>(Actor))
		{
			Hash = HashCombine(Hash, GetTypeHash(Actor));
			Hash = HashCombine(Hash, GetTypeHash(Actor->GetActorNameOrLabel()));
			for (const FGeneratorWrapper& Wrapper : Combination->Generators)
			{
				Hash = HashCombine(Hash, GetTypeHash(Wrapper.Generator.Get()));
				Hash = HashCombine(Hash, GetTypeHash((int32) Wrapper.bIsEnabled));
			}
		}
		else if (Actor->Implements<ULCGenerator>() && !IsExcluded(Actor))
		{
			Hash = HashCombine(Hash, GetTypeHash(Actor));
		}
	}
	return Hash;
}

void SGeneratorsStatus::Rebuild()
{
	Tabs.Reset();
	LevelCoordinates.Reset();
	TabLabels.Reset();
	LastSignature = ComputeSignature();

	if (UWorld* World = GetTargetWorld())
	{
		TArray<ALandscapeCombination*> Combinations;
		TSet<AActor*> AllGenerators;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!IsValid(Actor)) continue;

			if (Actor->IsA<ALevelCoordinates>()) LevelCoordinates = Actor;
			else if (ALandscapeCombination* Combination = Cast<ALandscapeCombination>(Actor)) Combinations.Add(Combination);
			else if (Actor->Implements<ULCGenerator>() && !IsExcluded(Actor)) AllGenerators.Add(Actor);
		}
		Combinations.Sort([](const ALandscapeCombination& A, const ALandscapeCombination& B) {
			return A.GetActorNameOrLabel() < B.GetActorNameOrLabel();
		});

		TSet<AActor*> UsedGenerators;
		for (ALandscapeCombination* Combination : Combinations)
		{
			FTab& Tab = Tabs.AddDefaulted_GetRef();
			Tab.Label = Combination->GetActorNameOrLabel();
			Tab.TabActor = Combination;

			for (int32 Index = 0; Index < Combination->Generators.Num(); ++Index)
			{
				AActor* Generator = Combination->Generators[Index].Generator.Get();
				if (!AsGenerator(Generator) || IsExcluded(Generator)) continue;

				UsedGenerators.Add(Generator);
				Tab.Generators.Add({ Generator, Index });
			}
		}

		TArray<AActor*> Others;
		for (AActor* Generator : AllGenerators)
		{
			if (!UsedGenerators.Contains(Generator)) Others.Add(Generator);
		}
		if (Others.Num() > 0)
		{
			Others.Sort([](const AActor& A, const AActor& B) {
				return A.GetActorNameOrLabel() < B.GetActorNameOrLabel();
			});

			FTab& Tab = Tabs.AddDefaulted_GetRef();
			Tab.Label = TEXT("Other");
			for (AActor* Generator : Others) Tab.Generators.Add({ Generator, INDEX_NONE });
		}
	}

	for (const FTab& Tab : Tabs) TabLabels.Add(MakeShared<FString>(Tab.Label));

	ActiveTab = FMath::Clamp(ActiveTab, 0, FMath::Max(0, Tabs.Num() - 1));
	if (TabCombo.IsValid())
	{
		TabCombo->RefreshOptions();
		if (TabLabels.IsValidIndex(ActiveTab)) TabCombo->SetSelectedItem(TabLabels[ActiveTab]);
	}

	RebuildRows();
}

void SGeneratorsStatus::RebuildRows()
{
	if (!RowsBox.IsValid()) return;
	RowsBox->ClearChildren();
	if (!Tabs.IsValidIndex(ActiveTab)) return;

	ALandscapeCombination* Combination = GetActiveCombination();
	for (const FRow& Row : Tabs[ActiveTab].Generators)
	{
		if (AActor* Actor = Row.Actor.Get())
		{
			RowsBox->AddSlot().AutoHeight().Padding(0, 1)
			[
				MakeRow(Actor, Combination, Row.Index)
			];
		}
	}
}

void SGeneratorsStatus::SelectTab(int32 Index)
{
	if (!Tabs.IsValidIndex(Index)) return;

	ActiveTab = Index;
	RebuildRows();
	OnGeneratorPicked.ExecuteIfBound(Tabs[Index].TabActor.Get());
}

void SGeneratorsStatus::PickActor(AActor* Actor)
{
	if (!IsValid(Actor)) return;

#if WITH_EDITOR
	if (GEditor)
	{
		GEditor->SelectNone(true, true);
		GEditor->SelectActor(Actor, true, true);
	}
#endif

	OnGeneratorPicked.ExecuteIfBound(Actor);
}

void SGeneratorsStatus::MoveGenerator(int32 From, int32 To)
{
	ALandscapeCombination* Combination = GetActiveCombination();
	if (!Combination || From == To || !Combination->Generators.IsValidIndex(From) || !Combination->Generators.IsValidIndex(To)) return;

	Combination->Modify();
	const FGeneratorWrapper Wrapper = Combination->Generators[From];
	Combination->Generators.RemoveAt(From);
	Combination->Generators.Insert(Wrapper, To);
	Rebuild();
}

void SGeneratorsStatus::RemoveGenerator(int32 Index)
{
	ALandscapeCombination* Combination = GetActiveCombination();
	if (!Combination || !Combination->Generators.IsValidIndex(Index)) return;

	Combination->Modify();
	Combination->Generators.RemoveAt(Index);
	Rebuild();
}

TSharedRef<SWidget> SGeneratorsStatus::MakeRow(AActor* Actor, ALandscapeCombination* Combination, int32 Index)
{
	TWeakObjectPtr<AActor> Weak(Actor);
	TWeakObjectPtr<ALandscapeCombination> WeakCombination(Combination);
	const EVisibility CombinationVisibility = Combination ? EVisibility::Visible : EVisibility::Collapsed;
	auto IsEnabled = [WeakCombination, Index]()
	{
		return !WeakCombination.IsValid() || !WeakCombination->Generators.IsValidIndex(Index) || WeakCombination->Generators[Index].bIsEnabled;
	};

	return SNew(SGeneratorRow)
	.Index(Index)
	.Label(FText::FromString(Actor->GetActorNameOrLabel()))
	.OnMove(FOnMoveGenerator::CreateSP(this, &SGeneratorsStatus::MoveGenerator))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SImage)
			.Image(LCIcons::Grip())
			.Visibility(CombinationVisibility)
		]
		+ SHorizontalBox::Slot().FillWidth(1.f)
		[
			SNew(SButton)
			.ButtonStyle(&RowButtonStyle()).ContentPadding(FMargin(6.f, 1.f))
			.ButtonColorAndOpacity_Lambda([Weak, IsEnabled]() -> FSlateColor { return RowColor(Weak.Get(), IsEnabled()); })
			.ToolTipText_Lambda([Weak]() -> FText { return FText::FromString(StatusString(Weak.Get())); })
			.Visibility_Lambda([Weak]() { return Weak.IsValid() ? EVisibility::Visible : EVisibility::Collapsed; })
			.OnClicked_Lambda([this, Weak]()
			{
				PickActor(Weak.Get());
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 4, 0)
				[
					SNew(SCheckBox)
					.Visibility(CombinationVisibility)
					.IsChecked_Lambda([IsEnabled]() { return IsEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([WeakCombination, Index](ECheckBoxState State)
					{
						if (!WeakCombination.IsValid() || !WeakCombination->Generators.IsValidIndex(Index)) return;
						WeakCombination->Modify();
						WeakCombination->Generators[Index].bIsEnabled = State == ECheckBoxState::Checked;
					})
				]
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					.Text_Lambda([Weak]() -> FText
					{
						return Weak.IsValid() ? FText::FromString(Weak->GetActorNameOrLabel()) : FText::GetEmpty();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
					.Visibility_Lambda([Weak]()
					{
						int32 NumGenerated = 0, NumPending = 0;
						return GetTiles(Weak.Get(), NumGenerated, NumPending) ? EVisibility::Visible : EVisibility::Hidden;
					})
					.Text_Lambda([Weak]() -> FText
					{
						int32 NumGenerated = 0, NumPending = 0;
						if (!GetTiles(Weak.Get(), NumGenerated, NumPending)) return FText::GetEmpty();
						return FText::FromString(FString::Printf(TEXT("%d/%d"), NumGenerated, NumGenerated + NumPending));
					})
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					MakeControls([Weak]() -> AActor* { return Weak.Get(); })
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					MakeIconButton(LCIcons::Unlink(), INVTEXT("Remove from the combination"),
						[CombinationVisibility]() { return CombinationVisibility; },
						[]() { return false; },
						[this, Index]() { RemoveGenerator(Index); })
				]
			]
		]
	];
}

TSharedRef<SWidget> SGeneratorsStatus::MakeAddMenu()
{
	FMenuBuilder Menu(true, nullptr);
	for (UClass* Class : GetGeneratorClasses())
	{
		Menu.AddMenuEntry(
			FText::FromString(Class->GetName()),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, Class]() { AddGenerator(Class); }))
		);
	}
	return Menu.MakeWidget();
}

void SGeneratorsStatus::AddGenerator(UClass* Class)
{
	UWorld* World = GetTargetWorld();
	if (!World || !Class) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ALandscapeCombination* Combination = GetActiveCombination();
	const FVector SpawnLocation = Combination ? Combination->GetActorLocation() : FVector::ZeroVector;

	AActor* NewActor = World->SpawnActor<AActor>(Class, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
#if WITH_EDITOR
	if (AVolume* Volume = Cast<AVolume>(NewActor))
	{
		UCubeBuilder* Builder = NewObject<UCubeBuilder>(GetTransientPackage(), NAME_None, RF_Transactional);
		Builder->X = Builder->Y = Builder->Z = 200.f;
		UActorFactory::CreateBrushForVolumeActor(Volume, Builder);
	}
#endif
	if (!IsValid(NewActor)) return;

	if (Combination)
	{
		FGeneratorWrapper NewWrapper;
		NewWrapper.Generator = NewActor;
		NewWrapper.bIsEnabled = true;
		Combination->Generators.Add(NewWrapper);
		Combination->Modify();
	}

	Rebuild();
	PickActor(NewActor);
}

FLinearColor SGeneratorsStatus::GetStatusColor(AActor* Actor)
{
	return RowColor(Actor);
}

float SGeneratorsStatus::GetUIScale()
{
	if (SavedScale <= 0.f)
	{
		float Value = 1.f;
		if (GConfig) GConfig->GetFloat(ScaleSection, ScaleKey, Value, ScaleIni());
		SavedScale = FMath::Clamp(Value, 0.5f, 2.f);
	}
	return SavedScale;
}

void SGeneratorsStatus::SetUIScale(float Scale)
{
	SavedScale = FMath::Clamp(Scale, 0.5f, 2.f);
	if (GConfig)
	{
		GConfig->SetFloat(ScaleSection, ScaleKey, SavedScale, ScaleIni());
		GConfig->Flush(false, ScaleIni());
	}
}

TSharedRef<SWidget> SGeneratorsStatus::MakeLevelCoordinatesRow()
{
	return SNew(SButton)
	.ButtonStyle(&RowButtonStyle())
	.ContentPadding(FMargin(6.f, 1.f))
	.ButtonColorAndOpacity(FLinearColor::FromSRGBColor(FColor(90, 90, 110)))
	.ToolTipText_Lambda([this]() -> FText
	{
		return LevelCoordinates.IsValid() ? INVTEXT("Select the Level Coordinates actor") : INVTEXT("Spawn a Level Coordinates actor at the origin");
	})
	.OnClicked_Lambda([this]()
	{
		if (AActor* Existing = LevelCoordinates.Get()) PickActor(Existing);
		else AddLevelCoordinates();
		return FReply::Handled();
	})
	[
		SNew(STextBlock)
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		.Text_Lambda([this]() -> FText
		{
			AActor* Existing = LevelCoordinates.Get();
			return Existing ? FText::FromString(Existing->GetActorNameOrLabel()) : INVTEXT("+ Add Level Coordinates");
		})
	];
}

void SGeneratorsStatus::AddLevelCoordinates()
{
	UWorld* World = GetTargetWorld();
	if (!World || LevelCoordinates.IsValid()) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* NewActor = World->SpawnActor<AActor>(ALevelCoordinates::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	if (!IsValid(NewActor)) return;

#if WITH_EDITOR
	NewActor->SetActorLabel(TEXT("LevelCoordinates"));
#endif

	Rebuild();
	PickActor(NewActor);
}
