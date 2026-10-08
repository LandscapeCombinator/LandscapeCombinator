// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "LandscapeCombinator/GeneratorButtons.h"
#include "LCCommon/LCPositionBasedGeneration.h"
#include "LCCommon/LCGenerator.h"
#include "GeneratorStatus.generated.h"

UCLASS(BlueprintType, Blueprintable)
class LANDSCAPECOMBINATOR_API UGeneratorStatus : public UUserWidget
{
	GENERATED_BODY()

public:

	// The ILCGenerator actor to monitor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeneratorStatus")
	TWeakObjectPtr<AActor> TargetGenerator;

	// How often to refresh, in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeneratorStatus")
	float RefreshInterval = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeneratorStatus")
	float SpinSpeed = 360.0f; // degrees per second

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> RowButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TilesText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> GenerateButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CancelButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CleanButton;

    UFUNCTION(BlueprintImplementableEvent, Category = "GeneratorStatus")
    void OnStatusRefreshed(int32 NumTilesGenerated, int32 NumTilesPending, bool bIsGenerating);

	UFUNCTION(BlueprintImplementableEvent, Category = "GeneratorStatus")
	void OnGeneratorClicked();

protected:

	FGeneratorButtons Buttons;

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void RefreshStatus();
	void UpdateRowColor(EGeneratorStatus GeneratorStatus, int32 NumGenerated, int32 NumPending);

	UFUNCTION()
	void HandleRowClicked();

	UFUNCTION()
	void HandleGenerateClicked();

	UFUNCTION()
	void HandleCancelClicked();

	UFUNCTION()
	void HandleCleanClicked();

	float TimeSinceLastRefresh = 0.0f;
};
