// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Coordinates/GlobalCoordinates.h"
#include "ImageDownloader/ImageDownloader.h"
#include "ConcurrencyHelpers/Concurrency.h"
#include "ConsoleHelpers/ExternalTool.h"
#include "LCCommon/LCGenerator.h"
#include "LCCommon/ActorSelection.h"

#include "GenericPlatform/GenericPlatformMisc.h"
#include "CoreMinimal.h"
#include "Landscape.h"
#include "Engine/DecalActor.h"
#include "WorldPartition/WorldPartition.h"
#include "Kismet/GameplayStatics.h"
#include "Landscape.h"
#include "LandscapeStreamingProxy.h"
#include "HAL/ThreadManager.h"

#include "LandscapeSpawner.generated.h"

#define LOCTEXT_NAMESPACE "FLandscapeCombinatorModule"

UENUM(BlueprintType)
enum class EComponentsMethod : uint8
{
	/* Enter the Quads Per Section, Section per Components, and Components Counts manually */
	Manual,

	/* When the landscape components do not exactly match with the total size of the heightmaps,
	 * Unreal Engine adds some padding at the border. Check this option if you are willing to
	 * drop some data at the border in order to make your heightmaps match the landscape
	 * components. */
	AutoWithoutBorder,
	
	/* Let Unreal Engine decide the component count based on the heightmap resolution. */
	Auto,

	Preset_8129_8129,
	Preset_4033_4033,
	Preset_2017_2017,
	Preset_1009_1009,
	Preset_1009_1009_2,
	Preset_505_505,
	Preset_505_505_2,
	Preset_253_253,
	Preset_253_253_2,
	Preset_127_127,
	Preset_127_127_2,
};

UENUM(BlueprintType)
enum class EDecalCreation : uint8
{
	None,
	Mapbox,
	MapTiler UMETA(DisplayName = "MapTiler"),
};

UENUM(BlueprintType)
enum class ESpawnMethod : uint8
{
	CreateFreshLandscape,

	CreateFreshLandscapeIncrementally,

	/**
	 * This option lets you extend an existing landscape. It will apply the heightmaps to the
	 * landscape. It will add components as necessary to extend the landscape if the downloaded
	 * heightmaps extend beyond the current landscape size. The landscape needs to be already placed
	 * in the rough correct location for the downloaded heightmaps.
	 */
	ExtendExistingLandscape
};

UCLASS(BlueprintType)
class LANDSCAPECOMBINATOR_API ALandscapeSpawner : public AActor, public ILCGenerator
{
	GENERATED_BODY()

public:
	ALandscapeSpawner();
	
	virtual bool ConfigureForTiles(int Zoom, int MinX, int MaxX, int MinY, int MaxY) override
	{
		if (IsValid(HeightmapDownloader)) return HeightmapDownloader->ConfigureForTiles(Zoom, MinX, MaxX, MinY, MaxY);
		else
		{
			LCReporter::ShowError(LOCTEXT("Error", "HeightmapDownloader is not set"));
			return false;
		}
	}

	virtual TArray<UObject*> GetGeneratedObjects() const override;

	/********************
	 * General Settings *
	 ********************/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (DisplayPriority = "-100")
	)
	ESpawnMethod SpawnMethod = ESpawnMethod::CreateFreshLandscape;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (EditCondition = "SpawnMethod == ESpawnMethod::ExtendExistingLandscape", EditConditionHides, DisplayPriority = "-50")
	)
	TObjectPtr<ALandscape> LandscapeToExtend = nullptr;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (EditCondition = "SpawnMethod != ESpawnMethod::ExtendExistingLandscape", EditConditionHides, DisplayPriority = "-50")
	)
	/* Label of the landscape to create. */
	FString LandscapeLabel = "SpawnedLandscape";

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (EditCondition = "SpawnMethod != ESpawnMethod::ExtendExistingLandscape", EditConditionHides, DisplayPriority = "-49")
	)
	/* Tag to add on the spawned landscape. */
	FName LandscapeTag = "lc";

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (EditCondition = "SpawnMethod != ESpawnMethod::ExtendExistingLandscape", EditConditionHides, DisplayPriority = "-48")
	)
	/* Material to apply to the landscape */
	TObjectPtr<UMaterialInterface> LandscapeMaterial = nullptr;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (EditCondition = "SpawnMethod != ESpawnMethod::ExtendExistingLandscape", EditConditionHides, DisplayPriority = "-47")
	)
	/**
	 * If you are using World Partition, check this option if you want to create landscape streaming proxies.
	 * This is useful if you have a large landscape, but it might slow things down for small landscapes.
	 * When using this option, it's important to use a preset for the component counts, or the generation
	 * will freeze or be very slow.
	 */
	bool bCreateLandscapeStreamingProxies = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (DisplayPriority = "4")
	)
	/* Folder used to spawn the actors. This setting is unused when generating from a combination or from blueprints. */
	FName SpawnedActorsPath;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "General",
		meta = (DisplayPriority = "5")
	)
	/* The scale in the Z-axis of your heightmap, ZScale = 1 corresponds to real-world size. */
	double ZScale = 1;


	/************
	 * Blending *
	 ************/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Blending",
		meta = (DisplayPriority = "0")
	)
	/**
	 * This is used if you want to create a "hole" in another, larger background landscape, so that the
	 * newly spawned landscape doesn't get hidden by the background landscape.
	 */
	bool bBlendLandscapeWithAnotherLandscape = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Blending",
		meta = (EditCondition = "bBLendLandscapeWithAnotherLandscape", EditConditionHides, DisplayPriority = "1")
	)
	FActorSelection LandscapeToBlendWith;

	/**********
	 * Decals *
	 **********/

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (DisplayPriority = "-10")
	)
	EDecalCreation DecalCreation = EDecalCreation::None;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (EditCondition = "DecalCreation != EDecalCreation::None", EditConditionHides, DisplayPriority = "-5")
	)
	bool bDecalMergeImages = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (EditCondition = "DecalCreation != EDecalCreation::None", EditConditionHides, DisplayPriority = "0")
	)
	int DecalsSortOrder = 0;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (EditCondition = "DecalCreation == EDecalCreation::Mapbox && !HasMapboxToken()", EditConditionHides, DisplayPriority = "1")
	)
	FString Decals_Mapbox_Token = "";

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (EditCondition = "DecalCreation == EDecalCreation::Mapbox", EditConditionHides, DisplayPriority = "2")
	)
	int Decals_Mapbox_Zoom = 9;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (EditCondition = "DecalCreation == EDecalCreation::Mapbox", EditConditionHides, DisplayPriority = "3")
	)
	bool Decals_Mapbox_2x = false;

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (EditCondition = "DecalCreation == EDecalCreation::MapTiler && !HasMapTilerToken()", EditConditionHides, DisplayPriority = "1")
	)
	FString Decals_MapTiler_Token = "";

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "Decals",
		meta = (EditCondition = "DecalCreation == EDecalCreation::MapTiler", EditConditionHides, DisplayPriority = "2")
	)
	int Decals_MapTiler_Zoom = 9;
	
	

	/********************
	 * Components Count *
	 ********************/	

	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ComponentCount",
		meta = (
			EditCondition = "SpawnMethod != ESpawnMethod::ExtendExistingLandscape",
			EditConditionHides, DisplayPriority = "3"
		)
	)
	EComponentsMethod ComponentsMethod = EComponentsMethod::AutoWithoutBorder;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ComponentCount",
		meta = (
			EditCondition = "SpawnMethod != ESpawnMethod::ExtendExistingLandscape && ComponentsMethod != EComponentsMethod::Auto && ComponentsMethod != EComponentsMethod::AutoWithoutBorder",
			EditConditionHides, DisplayPriority = "4"
		)
	)
	int QuadsPerSubsection;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ComponentCount",
		meta = (
			EditCondition = "SpawnMethod != ESpawnMethod::ExtendExistingLandscape && ComponentsMethod != EComponentsMethod::Auto && ComponentsMethod != EComponentsMethod::AutoWithoutBorder",
			EditConditionHides, DisplayPriority = "5"
		)
	)
	int SectionsPerComponent;
	
	UPROPERTY(
		EditAnywhere, BlueprintReadWrite, Category = "ComponentCount",
		meta = (
			EditCondition = "SpawnMethod != ESpawnMethod::ExtendExistingLandscape && ComponentsMethod != EComponentsMethod::Auto && ComponentsMethod != EComponentsMethod::AutoWithoutBorder",
			EditConditionHides, DisplayPriority = "6"
		)
	)
	FIntPoint ComponentCount;
	
	UPROPERTY(
		VisibleAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "10", ShowOnlyInnerProperties)
	)
	TObjectPtr<UImageDownloader> HeightmapDownloader = nullptr;
	
	UPROPERTY(BlueprintReadWrite, Category = "LandscapeSpawner")
	TObjectPtr<UImageDownloader> DecalDownloader = nullptr;

	UPROPERTY(
		VisibleAnywhere, BlueprintReadWrite, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "11", ShowOnlyInnerProperties)
	)
	TObjectPtr<ULCPositionBasedGeneration> PositionBasedGeneration = nullptr;
	
	/***********
	 * Actions *
	 ***********/
	
#if WITH_EDITOR

	/* Spawn the Landscape. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "-1")
	)
	void SpawnLandscape() {
		GenerateFromGameThread(SpawnedActorsPath, true);
	};

	bool SpawnLandscape(FName SpawnedActorsPathOverride, bool bIsUserInitiated, TObjectPtr<ALandscape>& OutLandscape, TArray<ADecalActor*>& OutDecals);
	virtual bool OnGenerate(FName SpawnedActorsPathOverride, bool bIsUserInitiated) override;

	virtual bool Cleanup_Implementation(bool bSkipPrompt) override
	{
		Modify();

		if (DeleteGeneratedObjects(bSkipPrompt))
		{
			SpawnedLandscape = nullptr;
			DecalActors.Empty();
			SpawnedLandscapeStreamingProxies.Empty();
			return true;
		}
		else
		{
			return false;
		}
	}
	virtual AActor* Duplicate(FName FromName, FName ToName) override;

#endif
	
	/* Delete the previously spawned landscape and decal. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (DisplayPriority = "0")
	)
	void DeleteLandscape();

	/* Click this to force reloading the WMS Provider from the URL */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (EditCondition = "IsWMS()", EditConditionHides, DisplayPriority = "4")
	)
	void ForceReloadWMS()
	{
		if (HeightmapDownloader) HeightmapDownloader->ForceReloadWMS();
	}

	/* Click this to set the min and max coordinates to the largest possible bounds */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (EditCondition = "IsWMS()", EditConditionHides, DisplayPriority = "5")
	)
	void SetLargestPossibleCoordinates()
	{
		if (HeightmapDownloader) HeightmapDownloader->SetLargestPossibleCoordinates();
	}

	/* Click this to set the WMS coordinates from a Location Volume or any other actor*/
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "LandscapeSpawner",
		meta = (EditCondition = "IsWMS()", EditConditionHides, DisplayPriority = "6")
	)
	void SetSourceParameters()
	{
		if (HeightmapDownloader) HeightmapDownloader->SetSourceParameters();
	}
	
	UFUNCTION()
	void SetComponentCountFromMethod();

#if WITH_EDITOR

	void PostEditChangeProperty(struct FPropertyChangedEvent&);

 #endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, DuplicateTransient, Category = "LandscapeSpawner",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TSoftObjectPtr<ALandscape> SpawnedLandscape = nullptr;

	UPROPERTY(EditAnywhere, DuplicateTransient, Category = "LandscapeSpawner",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TArray<TSoftObjectPtr<ALandscapeStreamingProxy>> SpawnedLandscapeStreamingProxies;
	
	UPROPERTY(EditAnywhere, DuplicateTransient, Category = "LandscapeSpawner",
		meta = (EditCondition = "false", EditConditionHides)
	)
	TArray<TSoftObjectPtr<ADecalActor>> DecalActors;

protected:

	UFUNCTION()
	bool IsWMS()
	{
		return IsValid(HeightmapDownloader) && HeightmapDownloader->IsWMS();
	}

	UFUNCTION()
	bool IsMapbox()
	{
		return IsValid(HeightmapDownloader) && HeightmapDownloader->IsMapbox();
	}

	UFUNCTION()
	static bool HasMapboxToken();

	UFUNCTION()
	static bool HasMapTilerToken();
};

#undef LOCTEXT_NAMESPACE