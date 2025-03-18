// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Engine/DecalActor.h"

#include "DecalCoordinates.generated.h"

#define LOCTEXT_NAMESPACE "FCoordinatesModule"

UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class COORDINATES_API UDecalCoordinates : public UActorComponent
{
	GENERATED_BODY()

public:

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "DecalCoordinates",
		meta = (DisplayPriority = "1")
	)
	FString PathToGeoreferencedImage;

	/* Move the Decal Actor to the correct position with respect to the global coordinate system. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "DecalCoordinates",
		meta = (DisplayPriority = "5")
	)
	void PlaceDecal();

	void PlaceDecal(FVector4d &OutCoordinates);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DecalCoordinates|Internal")
	TObjectPtr<UTexture2D> Texture;

	static ADecalActor* CreateDecal(UWorld *World, FString Path);
	static TArray<ADecalActor*> CreateDecals(UWorld *World, TArray<FString> Paths);
	static ADecalActor* CreateDecal(UWorld *World, FString Path, FVector4d &OutCoordinates);
};

#undef LOCTEXT_NAMESPACE