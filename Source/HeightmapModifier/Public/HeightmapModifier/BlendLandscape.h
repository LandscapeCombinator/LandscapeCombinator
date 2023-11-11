// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ConsoleHelpers/ExternalTool.h"

#include "CoreMinimal.h"
#include "Landscape.h"

#include "BlendLandscape.generated.h"

UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class HEIGHTMAPMODIFIER_API UBlendLandscape : public UActorComponent
{
	GENERATED_BODY()

public:
	UBlendLandscape();
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "BlendLandscape",
		meta = (DisplayPriority = "19")
	)
	ALandscape *LandscapeToBlendWith;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "BlendLandscape",
		meta = (DisplayPriority = "20")
	)
	bool bAdvancedBlendingOptions;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "BlendLandscape",
		meta = (EditCondition = "bAdvancedBlendingOptions", EditConditionHides, DisplayPriority = "21")
	)
	/* A curve that specifies how the data at the border of this landscape gets degraded into the other landscape.
	 * The X axis of the curve represents the distance from the border of the overlapping region and goes from 0 (at the border) to (1 at the center).
	 * The Y axis (Alpha) defines how we compute the new heightmap data:
	 * NewData = Alpha * OldData + (1 - Alpha) * OverlappingLandscapeData
	 * For example, a curve which is always 1 means that this landscape data is not changed. */
	TObjectPtr<UCurveFloat> DegradeThisData;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "BlendLandscape",
		meta = (EditCondition = "bAdvancedBlendingOptions", EditConditionHides, DisplayPriority = "22")
	)
	/* A curve that specifies how the data at the border of a landscape overlapping with this one gets modified.
	 * The X axis of the curve represents the distance from the border of the overlapping region and goes from 0 (at the border) to (1 at the center).
	 * The Y axis (Alpha) defines how we compute the new heightmap data of the overlapping landscape.
	 * For example, a curve which is always 1 means that the overlapping landscape data is not changed.
	 * In addition, the data of the overlapping landscape will not be changed in the positions where this landscape's
	 * data is equal to `ThisLandscapeNoData`.
	 * NewData = ThisData == ThisLandscapeNoData ? OldData : Alpha * OldData + (1 - Alpha) * OtherLandscapeNoData */
	TObjectPtr<UCurveFloat> DegradeOtherData;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "BlendLandscape",
		meta = (EditCondition = "bAdvancedBlendingOptions", EditConditionHides, DisplayPriority = "23")
	)
	/* Please check the tooltip of `DegradeOtherData` for documentation on how to use this value. */
	double ThisLandscapeNoData = 0;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "BlendLandscape",
		meta = (EditCondition = "bAdvancedBlendingOptions", EditConditionHides, DisplayPriority = "24")
	)
	/* Please check the tooltip of `DegradeOtherData` for documentation on how to use this value. */
	double OtherLandscapeNoData = 0;
	
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "BlendLandscape")
	void BlendWithLandscape();

};
