// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeneratorWrapper.generated.h"

UENUM(BlueprintType)
enum class EGeneratorStatus : uint8
{
	Idle,
	Generating,
	Error,
	Success
};

USTRUCT(BlueprintType)
struct FGeneratorWrapper
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeneratorWrapper", meta = (DisplayPriority = "0"))
	bool bIsEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeneratorWrapper", meta = (DisplayPriority = "1", MustImplement = "/Script/LCCommon.LCGenerator"))
	TSoftObjectPtr<AActor> Generator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GeneratorWrapper", meta = (DisplayPriority = "2"))
	EGeneratorStatus GeneratorStatus = EGeneratorStatus::Idle;
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
