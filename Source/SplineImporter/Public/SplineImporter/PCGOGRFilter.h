// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "OGRGeometry.h"
#include "CoreMinimal.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "PCGContext.h"
#include "Misc/EngineVersionComparison.h"

#include "GDALInterface/GDALInterface.h"

#include "PCGOGRFilter.generated.h"

struct FPCGOGRFilterContext : public FPCGContext
{
	TObjectPtr<UGlobalCoordinates> GlobalCoordinates;
};

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (PCG_Overridable))
	TObjectPtr<AOGRGeometry> GeometryActor;
};

#if UE_VERSION_OLDER_THAN(5, 4, 0)
class FPCGOGRFilterElement : public FSimplePCGElement
#elif UE_VERSION_OLDER_THAN(5, 5, 0)
class FPCGOGRFilterElement : public IPCGElement
#else
class FPCGOGRFilterElement : public IPCGElementWithCustomContext<FPCGOGRFilterContext>
#endif
{
public:
#if UE_VERSION_OLDER_THAN(5, 4, 0)
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override
	{
		FPCGOGRFilterContext* Context = new FPCGOGRFilterContext();
		Context->InputData = InputData;
		Context->SourceComponent = SourceComponent;
		Context->Node = Node;
		return Context;
	}
#elif UE_VERSION_OLDER_THAN(5, 5, 0)
	virtual FPCGContext* CreateContext() override { return new FPCGOGRFilterContext(); }
#endif

protected:
	virtual bool PrepareDataInternal(FPCGContext* Context) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const
	{
		return Context && Context->CurrentPhase == EPCGExecutionPhase::PrepareData;
	}
};
