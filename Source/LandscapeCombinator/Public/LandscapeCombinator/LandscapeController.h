// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "LandscapeController.generated.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

UCLASS(BlueprintType)
class LANDSCAPECOMBINATOR_API ULandscapeController : public UActorComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeCombinator")
	/* Adjust the landscape scale and position to respect the `LevelCoordinates` and the `ZScale`. */
	void AdjustLandscape();

	UPROPERTY(VisibleAnywhere, Category = "LandscapeCombinator|Information",
		meta = (DisplayPriority = "1")
	)
	FVector4d OriginalCoordinates;

	UPROPERTY(VisibleAnywhere, Category = "LandscapeCombinator|Information",
		meta = (DisplayPriority = "2")
	)
	int OriginalEPSG;
	
	UPROPERTY(VisibleAnywhere, Category = "LandscapeCombinator|Information",
		meta = (DisplayPriority = "3")
	)
	FVector2D Altitudes;
	
	UPROPERTY(VisibleAnywhere, Category = "LandscapeCombinator|Information",
		meta = (DisplayPriority = "4")
	)
	FIntPoint InsidePixels;
	
	UPROPERTY(EditAnywhere, Category = "LandscapeCombinator|Information",
		meta = (DisplayPriority = "5")
	)
	/* Please press `AdjustLandscape` if you modify the scale. */
	double ZScale;
};

#undef LOCTEXT_NAMESPACE