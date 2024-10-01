// Copyright 2023 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "ConsoleHelpers/ExternalTool.h"

#include "CoreMinimal.h"
#include "Landscape.h"

#include "HeightmapModifier.generated.h"

UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class HEIGHTMAPMODIFIER_API UHeightmapModifier : public UActorComponent
{
	GENERATED_BODY()

public:
	UHeightmapModifier();

#if WITH_EDITOR

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "HeightmapModifier|ExternalTool")
	void ApplyToolToHeightmap();

#endif

	UPROPERTY(EditAnywhere, Category = "HeightmapModifier|ExternalTool",
		meta = (DisplayPriority = "1")
	)
	/* Use a cube or another rectangular actor to bound the area on your landscape on which you want to apply the External Tool */
	AActor* BoundingActor;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "HeightmapModifier|ExternalTool",
		meta = (DisplayPriority = "11")
	)
	TObjectPtr<UExternalTool> ExternalTool;
};
