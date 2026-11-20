// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGContext.h"
#include "PCGElement.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "Misc/EngineVersionComparison.h"

#include "LCCommon/ActorSelection.h"
#include "SplineImporter/OGRGeometry.h"

#include "PCGOGRSurfaceSampler.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class SPLINEIMPORTER_API UPCGOGRSurfaceSamplerSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("OGRSurfaceSampler")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGOGRSurfaceSamplerElement", "NodeTitle", "OGR Surface Sampler"); }
	virtual FText GetNodeTooltipText() const override { return NSLOCTEXT("PCGOGRSurfaceSamplerElement", "NodeTooltip", "Samples the planet surface on a seamless lat/long lattice inside an OGR area, away from splines."); }
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
#if !UE_VERSION_OLDER_THAN(5, 7, 0)
	virtual void GetStaticTrackedKeys(FPCGSelectionKeyToSettingsMap& OutKeysToSettings, TArray<TObjectPtr<const UPCGGraph>>& OutVisitedGraphs) const override;
#endif
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return TArray<FPCGPinProperties>(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return Super::DefaultPointOutputPinProperties(); }
	virtual FPCGElementPtr CreateElement() const override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (PCG_Overridable))
	TObjectPtr<AOGRGeometry> GeometryActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (PCG_Overridable))
	FActorSelection SurfaceSelection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (PCG_Overridable, ClampMin = "1"))
	double SpacingCm = 1000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (PCG_Overridable, ClampMin = "0", ClampMax = "1"))
	double Jitter = 0.8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (PCG_Overridable))
	int32 GlobalSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (PCG_Overridable))
	double MinAltitudeCm = -1000000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings", meta = (PCG_Overridable))
	double MaxAltitudeCm = 1000000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exclusion", meta = (PCG_Overridable, ClampMin = "0"))
	double ExclusionRadiusCm = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exclusion", meta = (PCG_Overridable))
	TArray<FName> SplinesTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exclusion", meta = (PCG_Overridable))
	FName SplineComponentsTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", AdvancedDisplay, meta = (PCG_Overridable))
	int32 MaxCandidatesPerCell = 2000000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", AdvancedDisplay, meta = (PCG_Overridable, ClampMin = "0.5"))
	double TimeSliceMs = 4.0;
};

#if !UE_VERSION_OLDER_THAN(5, 7, 0)

struct FOGRSurfaceSamplerState;

struct FPCGOGRSurfaceSamplerContext : public FPCGContext
{
	TSharedPtr<FOGRSurfaceSamplerState> State;
};

class FPCGOGRSurfaceSamplerElement : public IPCGElementWithCustomContext<FPCGOGRSurfaceSamplerContext>
#else
class FPCGOGRSurfaceSamplerElement : public IPCGElement
#endif
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};
