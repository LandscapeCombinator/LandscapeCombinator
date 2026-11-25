// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "LCMaterialBinding.generated.h"

class UMaterialInterface;
class UMaterialInstance;
class UTexture;

USTRUCT()
struct FLCLayerTextureEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FString SourceKey;

	UPROPERTY()
	FName LayerName;

	UPROPERTY()
	int32 LayerIndex = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<UTexture> Texture = nullptr;

	/* Masks only go to the blend of the layer (Mask parameters), images go to the layer and its blend (Texture parameters) */
	UPROPERTY()
	bool bMask = false;

	UPROPERTY()
	FVector2D BoundsMin = FVector2D::ZeroVector;

	UPROPERTY()
	FVector2D BoundsMax = FVector2D::ZeroVector;
};

USTRUCT()
struct FLCBoundSlot
{
	GENERATED_BODY()

	UPROPERTY()
	FName ComponentName;

	UPROPERTY()
	int32 SlotIndex = 0;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BaseMaterial = nullptr;
};

UCLASS(ClassGroup = (LandscapeCombinator), meta = (BlueprintSpawnableComponent))
class COORDINATES_API ULCMaterialBinding : public UActorComponent
{
	GENERATED_BODY()

public:
	static const FName TextureParameterName;
	static const FName BoundsMinParameterName;
	static const FName BoundsMaxParameterName;
	static const FName MaskParameterName;
	static const FName MaskBoundsMinParameterName;
	static const FName MaskBoundsMaxParameterName;
	static const FName UseMaskParameterName;
	static const FName BodyParameterName;
	static const FName BodyLocationHighParameterName;
	static const FName BodyLocationLowParameterName;

	static bool BindTexture(
		AActor* Target, const FString& SourceKey, FName LayerName,
		UTexture* Texture, const FVector2D& BoundsMin, const FVector2D& BoundsMax, bool bMask = false
	);

	/* Key identifying Owner as a binding source: its instance GUID in the editor, its name in builds without actor GUIDs
	 * (AActor::GetActorInstanceGuid only exists in the editor) */
	static FString GetSourceKeyOf(const AActor* Owner);

	static void RemoveSource(AActor* Target, const FString& SourceKey);

	static void RemoveSourcesWithPrefix(AActor* Target, const FString& KeyPrefix);

	void Rebuild();

	/* Rebuilds on the next tick */
	void RequestRebuild();

	virtual void OnRegister() override;

private:
	UPROPERTY()
	TArray<FLCLayerTextureEntry> Entries;

	UPROPERTY()
	TArray<FLCBoundSlot> Slots;

	bool bRebuildPending = false;

	void FindSlots();
	UMaterialInstance* GetOrCreateInstance(const FLCBoundSlot& Slot);
	void WriteParameters(UMaterialInstance* Instance);
};
