// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "PCGPin.h"
#include "PCGSettings.h"

#pragma warning(disable: 4668)
#include "gdal.h"
#include "gdal_priv.h"
#include "gdal_utils.h"
#include <ogr_api.h>
#include <ogrsf_frmts.h>
#pragma warning(default: 4668)


#include "PCGOGRFilter.generated.h"

UENUM(BlueprintType)
enum class EFoliageSourceType : uint8 {
	LocalVectorFile,
	OverpassShortQuery,
	Forests
};

UCLASS(BlueprintType, ClassGroup = (Procedural))
class LANDSCAPECOMBINATOR_API UPCGOGRFilterSettings : public UPCGSettings
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
	OGRGeometry* GetGeometryFromShortQuery(FBox Bounds, FString ShortQuery) const;

public:
	OGRGeometry* GetGeometry(FBox Bounds) const;

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

class FPCGOGRFilterElement : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
