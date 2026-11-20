// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SComboBox.h"

class ALandscapeCombination;
class UWorld;

DECLARE_DELEGATE_OneParam(FOnGeneratorPicked, AActor*);

class LANDSCAPECOMBINATOR_API SGeneratorsStatus : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGeneratorsStatus) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UWorld>, World)
		SLATE_EVENT(FOnGeneratorPicked, OnGeneratorPicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	static FLinearColor GetStatusColor(AActor* Actor);

	static float GetUIScale();
	static void SetUIScale(float Scale);

private:
	struct FRow
	{
		TWeakObjectPtr<AActor> Actor;
		int32 Index;
	};

	struct FTab
	{
		FString Label;
		TWeakObjectPtr<AActor> TabActor;
		TArray<FRow> Generators;
	};

	TWeakObjectPtr<UWorld> RuntimeWorld;
	FOnGeneratorPicked OnGeneratorPicked;

	TArray<FTab> Tabs;
	TArray<TSharedPtr<FString>> TabLabels;
	int32 ActiveTab = 0;
	uint32 LastSignature = 0;
	float PendingScale = 1.f;
	bool bDraggingScale = false;

	TSharedPtr<SVerticalBox> RowsBox;
	TWeakObjectPtr<AActor> LevelCoordinates;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> TabCombo;

	UWorld* GetTargetWorld() const;
	ALandscapeCombination* GetActiveCombination() const;
	uint32 ComputeSignature() const;

	void Rebuild();
	void RebuildRows();
	void SelectTab(int32 Index);
	void PickActor(AActor* Actor);
	void AddGenerator(UClass* Class);
	void AddLevelCoordinates();
	void MoveGenerator(int32 From, int32 To);
	void RemoveGenerator(int32 Index);

	TSharedRef<SWidget> MakeRow(AActor* Actor, ALandscapeCombination* Combination, int32 Index);
	TSharedRef<SWidget> MakeAddMenu();
	TSharedRef<SWidget> MakeLevelCoordinatesRow();

	EActiveTimerReturnType OnRefreshTimer(double InCurrentTime, float InDeltaTime);
};
