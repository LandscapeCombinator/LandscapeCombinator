// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LCCommon/LCGenerator.h"

#if WITH_EDITOR
#include "DetailWidgetRow.h"
#endif

#include "GeneratorWrapper.generated.h"

USTRUCT(BlueprintType)
struct FGeneratorWrapper
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeneratorWrapper", meta = (DisplayPriority = "0"))
    bool bIsEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeneratorWrapper", meta = (DisplayPriority = "1", MustImplement = "/Script/LCCommon.LCGenerator"))
    TSoftObjectPtr<AActor> Generator;

    EGeneratorStatus GetStatus() const
    {
        if (Generator.IsValid() && Generator->Implements<ULCGenerator>())
        {
            return Cast<ILCGenerator>(Generator.Get())->GetGeneratorStatus();
        }
        return EGeneratorStatus::Idle;
    }
};

#if WITH_EDITOR

class FGeneratorWrapperCustomization : public IPropertyTypeCustomization
{
public:
    static TSharedRef<IPropertyTypeCustomization> MakeInstance();
    virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructHandle, FDetailWidgetRow& Row, IPropertyTypeCustomizationUtils&);
    virtual void CustomizeChildren(TSharedRef<IPropertyHandle>, IDetailChildrenBuilder&, IPropertyTypeCustomizationUtils&) override {}
};

#endif