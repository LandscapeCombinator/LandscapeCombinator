// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SplineImporter/GDALImporter.h"
#include "GDALInterface/GDALInterface.h"

#include "OGRGeometry.generated.h"

UCLASS(BlueprintType)
class SPLINEIMPORTER_API AOGRGeometry : public AGDALImporter
{
	GENERATED_BODY()

public:

	/* Tag to apply to the current actor when importing the area. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "5")
	)
	FName AreaTag;

	OGRGeometry *Geometry = nullptr;

	void Import(TFunction<void(bool)> OnComplete) override;

protected:
	virtual void SetOverpassShortQuery() override;
};
