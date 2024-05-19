// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "OGRGeometry.h"
#include "CoreMinimal.h"
#include "PCGPin.h"
#include "PCGSettings.h"

#include "GDALInterface/GDALInterface.h"

#include "PCGOGRFilter.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class SPLINEIMPORTER_API UPCGOGRFilterSettings : public UPCGSettings
{
	GENERATED_BODY()

public:

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("OGR Filter")); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Filter; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return Super::DefaultPointInputPinProperties(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = Settings,
		meta = (DisplayPriority = "0", PCG_Overridable)
	)
	FName GeometryTag;
};

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 4)
class FPCGOGRFilterElement : public IPCGElement
#else
class FPCGOGRFilterElement : public FSimplePCGElement
#endif
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
