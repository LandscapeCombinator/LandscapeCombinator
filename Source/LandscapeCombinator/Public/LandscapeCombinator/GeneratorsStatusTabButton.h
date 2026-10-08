// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GeneratorsStatusTabButton.generated.h"

class UGeneratorsStatus;

UCLASS(BlueprintType, Blueprintable)
class LANDSCAPECOMBINATOR_API UGeneratorsStatusTabButton : public UUserWidget
{
	GENERATED_BODY()

public:

	void InitializeTab(UGeneratorsStatus* InOwner, int32 InTabIndex, const FText& InLabel);
	void SetActive(bool bNewActive);

	UPROPERTY(BlueprintReadOnly, Category = "GeneratorsStatusTabButton")
	int32 TabIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "GeneratorsStatusTabButton")
	bool bIsActive = false;

    void ApplyColor();

	UPROPERTY(BlueprintReadWrite, Category = "GeneratorsStatusTabButton")
	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ButtonRoot;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LabelText;

	UFUNCTION(BlueprintImplementableEvent, Category = "GeneratorsStatusTabButton")
	void OnActiveChanged(bool bNewActive);

	UFUNCTION(BlueprintImplementableEvent, Category = "GeneratorsStatusTabButton")
	void OnLabelSet(const FText& NewLabel);

	void InitializeTab(UGeneratorsStatus* InOwner, int32 InTabIndex, const FText& InLabel, AActor* InTargetActor);

protected:

	UFUNCTION()
	void HandleClicked();

	UPROPERTY()
	TWeakObjectPtr<UGeneratorsStatus> Owner;
};
