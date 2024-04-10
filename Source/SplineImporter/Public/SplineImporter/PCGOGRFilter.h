// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGPin.h"
#include "PCGSettings.h"

#include "GDALInterface/GDALInterface.h"

#include "PCGOGRFilter.generated.h"

UENUM(BlueprintType)
enum class EFoliageSourceType : uint8
{
	LocalVectorFile,
	OverpassShortQuery,
	Forests
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


	OGRGeometry* GetGeometryFromPath(FString Path) const;
	OGRGeometry* GetGeometryFromQuery(FString Query) const;
	OGRGeometry* GetGeometryFromShortQuery(UWorld *World, FBox Bounds, FString ShortQuery) const;

public:
	OGRGeometry* GetGeometry(UWorld *World, FBox Bounds) const;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = Settings,
		meta = (DisplayPriority = "0")
	)
	EFoliageSourceType FoliageSourceType = EFoliageSourceType::Forests;


	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = Settings,
		meta = (EditCondition = "FoliageSourceType == EFoliageSourceType::LocalVectorFile", EditConditionHides, DisplayPriority = "1")
	)
	FString OSMPath;


	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = Settings,
		meta = (EditCondition = "FoliageSourceType == EFoliageSourceType::OverpassShortQuery", EditConditionHides, DisplayPriority = "2")
	)
	FString OverpassShortQuery = "nwr[\"landuse\"=\"forest\"];nwr[\"natural\"=\"wood\"];";
};

class FPCGOGRFilterElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
