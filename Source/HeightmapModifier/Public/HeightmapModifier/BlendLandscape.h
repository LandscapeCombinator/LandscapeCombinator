// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ConsoleHelpers/ExternalTool.h"
#include "LCCommon/ActorSelection.h"

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
	FActorSelection OtherLandscapeSelection;
	
	UPROPERTY(
		AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "BlendLandscape",
		meta = (DisplayPriority = "21")
	)
	/* A curve that specifies how the data at the border of this landscape gets lowered at the border
	 * The X axis of the curve represents the distance from the border of the overlapping region and goes from 0 (at the border) to (1 at the center).
	 * The Y axis (Alpha) defines how we compute the new heightmap data:
	 * NewData = Alpha * OldData
	 * For example, a curve which is always 1 means that this landscape data is not changed. */
	TObjectPtr<UCurveFloat> DegradeThisData;
	
	UPROPERTY(
		AdvancedDisplay, EditAnywhere, BlueprintReadWrite, Category = "BlendLandscape",
		meta = (DisplayPriority = "22")
	)
	/* A curve that specifies how the data at the border of a landscape overlapping with this one gets modified.
	 * The X axis of the curve represents the distance from the border of the overlapping region and goes from 0 (at the border) to (1 at the center).
	 * The Y axis (Alpha) defines how we compute the new heightmap data of the overlapping landscape.
	 * For example, a curve which is always 1 means that the overlapping landscape data is not changed.
	 * In addition, the data of the overlapping landscape will not be changed in the positions where this landscape's
	 * data is equal to `0`.
	 * NewData = ThisData == 0 ? OldData : Alpha * OldData */
	TObjectPtr<UCurveFloat> DegradeOtherData;
	
#if WITH_EDITOR

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "BlendLandscape")
	void BlendWithLandscape() { BlendWithLandscape(true); };

	bool BlendWithLandscape(bool bIsUserInitiated);

#endif

};
