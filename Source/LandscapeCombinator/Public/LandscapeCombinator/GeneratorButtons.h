// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Components/Button.h"

struct FGeneratorButtons
{
    bool bSpinning = false;
    float SpinAngle = 0.0f;

    // bAvailable == false hides all three buttons (e.g. nothing to control)
    void Update(UButton* Generate, UButton* Cancel, UButton* Clean, bool bAvailable, bool bGenerating)
    {
        bSpinning = bAvailable && bGenerating;
        const ESlateVisibility Idle = (bAvailable && !bGenerating) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
        const ESlateVisibility Busy = (bAvailable && bGenerating) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
        if (Generate) Generate->SetVisibility(bSpinning ? ESlateVisibility::HitTestInvisible : Idle);
        if (Clean) Clean->SetVisibility(Idle);
        if (Cancel) Cancel->SetVisibility(Busy);
    }

    // Call every frame
    void Tick(UButton* Generate, float SpinSpeed, float DeltaTime)
    {
        if (!Generate) return;
        if (bSpinning)
        {
            SpinAngle = FMath::Fmod(SpinAngle + SpinSpeed * DeltaTime, 360.0f);
            Generate->SetRenderTransformAngle(SpinAngle);
        }
        else if (SpinAngle != 0.0f)
        {
            SpinAngle = 0.0f;
            Generate->SetRenderTransformAngle(0.0f);
        }
    }
};
