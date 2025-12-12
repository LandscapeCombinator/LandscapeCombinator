// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GDALImporter",
		meta = (DisplayPriority = "6")
	)
	bool bClearGeometryBeforeImporting = true;

	OGRGeometry *Geometry = nullptr;

	bool OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated) override;

	virtual bool Cleanup_Implementation(bool bSkipPrompt) override {
		Modify();
		Geometry = nullptr;
		AlreadyHandledFeatures.Empty();
		return true;
	}

#if WITH_EDITOR
	virtual AActor* Duplicate(FName FromName, FName ToName) override;
#endif

protected:
	virtual void SetOverpassShortQuery() override;
};
