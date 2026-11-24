// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Engine/DecalActor.h"

#include "DecalCoordinates.generated.h"

class ALevelCoordinates;
class UGlobalCoordinates;

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
	bool PlaceDecal(UMaterialInterface *Material);

	bool PlaceDecal(UMaterialInterface *Material, FVector4d &OutCoordinates);

	virtual void OnRegister() override;
	bool ApplyDecalTexture();
	void RefreshSphereMapping(const UGlobalCoordinates* GlobalCoordinates);
	FString ReprojectedImagePath() const;
	bool bTextureApplied = false;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> DecalMaterial;

	UPROPERTY()
	bool bSphereDecal = false;

	UPROPERTY()
	FVector2D DecalBoundsMin = FVector2D::ZeroVector;

	UPROPERTY()
	FVector2D DecalBoundsMax = FVector2D::ZeroVector;

	UPROPERTY()
	FVector2D DecalWorldBoundsMin = FVector2D::ZeroVector;

	UPROPERTY()
	FVector2D DecalWorldBoundsMax = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DecalCoordinates|Internal")
	TObjectPtr<UTexture2D> Texture;

	static ADecalActor* CreateDecal(UWorld *World, UMaterialInterface *Material, FString Path);
	static TArray<ADecalActor*> CreateDecals(UWorld *World, UMaterialInterface *Material, TArray<FString> Paths, ALevelCoordinates* Body = nullptr);
	static ADecalActor* CreateDecal(UWorld *World, UMaterialInterface *Material, FString Path, FVector4d &OutCoordinates, ALevelCoordinates* Body = nullptr);

	static UMaterialInstanceDynamic* SetDecalTextureFromColors(
		UDecalComponent* Decal, UMaterialInterface* Material,
		int Width, int Height, TArray<FColor>& Colors, UObject* Outer, FString TextureName
	);
	static UTexture2D* CreateDecalTexture(int Width, int Height, TArray<FColor>& Colors, UObject* Outer, FString TextureName, bool bSaveWithOuter = false);
	static FString ReprojectedPath(const FString& Image);
	static bool ReprojectImage(const FString& Image, UGlobalCoordinates* GlobalCoordinates, FVector4d& OutCRSBounds, FVector4d& OutWorldBounds);
	static UTexture2D* CreateTextureFromImage(const FString& Image, UObject* Outer, const FString& TextureName);
	static UMaterialInstanceDynamic* SetDecalTexture(UDecalComponent* Decal, UMaterialInterface* Material, UTexture2D* DecalTexture, UObject* Outer);
};

#undef LOCTEXT_NAMESPACE