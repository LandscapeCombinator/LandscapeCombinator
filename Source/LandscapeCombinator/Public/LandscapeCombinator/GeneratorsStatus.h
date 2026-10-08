// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "GeneratorStatus.h"
#include "GeneratorsStatusTabButton.h"
#include "BuildingsFromSplines/Building.h"
#include "LandscapeCombinator/LandscapeCombination.h"
#include "LandscapeCombinator/LandscapePCGVolume.h"
#include "GeneratorsStatus.generated.h"

class ALandscapeCombination;

struct FGeneratorTabInfo
{
	TWeakObjectPtr<AActor> TabActor; // the ALandscapeCombination of the tab, null for "Other"
};

/**
 * Displays one tab per ALandscapeCombination in the world, showing that combination's generators
 * in the order they appear in its Generators array
 */
UCLASS(BlueprintType, Blueprintable)
class LANDSCAPECOMBINATOR_API UGeneratorsStatus : public UUserWidget
{
    GENERATED_BODY()

public:

    // Row widget class for each generator
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeneratorsStatus")
    TSubclassOf<UGeneratorStatus> GeneratorStatusClass;

    // Tab header button class to spawn per tab
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeneratorsStatus")
    TSubclassOf<UGeneratorsStatusTabButton> TabButtonClass;

    // Label used for the extra tab holding generators not part of any combination
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeneratorsStatus")
    FString OtherGeneratorsTabLabel = TEXT("Other");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeneratorsStatus")
    FLinearColor ActiveTabColor = FLinearColor(0.01f, 0.01f, 0.012f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeneratorsStatus")
    FLinearColor InactiveTabColor = FLinearColor(0.12f, 0.12f, 0.14f, 1.0f);

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> PanelBorder;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeneratorsStatus")
    float SpinSpeed = 360.0f;

    // Actor classes to always leave out of the tabs, e.g. ABuilding, ALandscapeCombination
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeneratorsStatus")
    TSet<TSubclassOf<AActor>> ExcludedGeneratorClasses = { ABuilding::StaticClass(), ALandscapeCombination::StaticClass() };

    UFUNCTION(BlueprintCallable, Category = "GeneratorsStatus")
    void RefreshGeneratorsList();

    UFUNCTION(BlueprintCallable, Category = "GeneratorsStatus")
    void SetActiveTab(int32 TabIndex);

	UFUNCTION(BlueprintCallable, Category = "GeneratorsStatus")
	void GenerateAll();

	UFUNCTION(BlueprintCallable, Category = "GeneratorsStatus")
	void CancelAll();

	UFUNCTION(BlueprintCallable, Category = "GeneratorsStatus")
	void CleanAll();

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> GenerateAllButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CancelAllButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CleanAllButton;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidgetSwitcher> TabSwitcher;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UHorizontalBox> TabButtonBox;

    UFUNCTION(BlueprintImplementableEvent, Category = "GeneratorsStatus")
    void OnTabChanged(int32 NewTabIndex);

protected:

    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    void UpdateAllButtons();

    bool IsGeneratorExcluded(AActor* Generator) const;
    ALandscapeCombination* GetActiveCombination() const;

	UFUNCTION()
	void HandleGenerateAllClicked();

	UFUNCTION()
	void HandleCancelAllClicked();

	UFUNCTION()
	void HandleCleanAllClicked();

	TArray<FGeneratorTabInfo> TabInfos;
	int32 ActiveTabIndex = 0;
    FGeneratorButtons Buttons;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UGeneratorsStatusTabButton>> TabButtons;
};
