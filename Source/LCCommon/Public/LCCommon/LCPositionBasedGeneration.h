// Copyright 2023-2027 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h" 
#include "LCPositionBasedGeneration.generated.h"

USTRUCT(BlueprintType)
struct LCCOMMON_API FTile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile")
	int Zoom;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile")
	int X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tile")
	int Y;

	FTile()
		: Zoom(0)
		, X(0)
		, Y(0)
	{}

	FTile(int InZoom, int InX, int InY)
		: Zoom(InZoom)
		, X(InX)
		, Y(InY)
	{}

	bool operator==(const FTile& Other) const
	{
		return Zoom == Other.Zoom && X == Other.X && Y == Other.Y;
	}

	friend uint32 GetTypeHash(const FTile& Other)
	{
		uint32 Hash = 0;
		Hash = FCrc::MemCrc32(&Other.Zoom, sizeof(int));
		Hash = FCrc::MemCrc32(&Other.X, sizeof(int), Hash);
		Hash = FCrc::MemCrc32(&Other.Y, sizeof(int), Hash);
		return Hash;
	}
};

USTRUCT(BlueprintType)
struct LCCOMMON_API FTileRecord
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<TSoftObjectPtr<UObject>> Objects;

	UPROPERTY(Transient)
	int LastUsedPass = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FLCOnTilesGenerated, const TArray<TSoftObjectPtr<UObject>>&);
DECLARE_MULTICAST_DELEGATE_OneParam(FLCOnTilesDestroyed, const TArray<TSoftObjectPtr<UObject>>&);

UCLASS(BlueprintType)
class LCCOMMON_API ULCPositionBasedGeneration : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(AdvancedDisplay, EditAnywhere, Category = "PositionBasedGeneration", meta=(DisplayPriority=1))
	bool bEnablePositionBasedGeneration = false;

	UPROPERTY(AdvancedDisplay, EditAnywhere, Category = "PositionBasedGeneration", meta=(DisplayPriority=4))
	int ZoomLevel = 14;

	UPROPERTY(AdvancedDisplay, EditAnywhere, Category = "PositionBasedGeneration", meta=(DisplayPriority=5))
	// Distance 0 means 1 tile, Distance 1 means 3x3=9 tiles, Distance 2 means 5x5=25 tiles, etc.
	int GenerateAllTilesAtDistance = 1;

	UPROPERTY(AdvancedDisplay, EditAnywhere, Category = "PositionBasedGeneration", meta=(DisplayPriority=6))
	bool bShowGenerationState = true;

	UPROPERTY(AdvancedDisplay, EditAnywhere, Category = "PositionBasedGeneration", meta=(DisplayPriority=7))
	// group the first group of tiles as a single rectangle
	bool bGroupFirstTiles = true;

	UPROPERTY(EditAnywhere, Category = "PositionBasedGeneration", meta = (ToolTip = "Use a longitude/latitude tile grid that reaches the poles, instead of Web Mercator. Supported for WMS, and for XYZ sources with Georeference Slippy Tiles enabled."))
	bool bGeodeticTiles = false;

	UPROPERTY(EditAnywhere, Category = "PositionBasedGeneration", meta = (ClampMin = "0", ToolTip = "Skip the generation while the player is lower than this altitude, in km."))
	double MinGenerationAltitudeKm = 0;

	UPROPERTY(EditAnywhere, Category = "PositionBasedGeneration", meta = (ClampMin = "0", ToolTip = "Skip the generation while the player is higher than this altitude, in km."))
	double MaxGenerationAltitudeKm = 0;

	UPROPERTY(EditAnywhere, Category = "PositionBasedGeneration", meta = (ClampMin = "0", ToolTip = "Maximum number of tiles kept generated, 0 for no limit. Above it, the least recently used tiles outside the generation range are destroyed. Tiles are generated one by one when this is set."))
	int MaxTiles = 0;

	UPROPERTY(EditAnywhere, Category = "PositionBasedGeneration", meta = (ToolTip = "Also generate around the player (or the editor viewport), in addition to the registered generation sources."))
	bool bUsePlayerPosition = true;

	UPROPERTY(EditAnywhere, Category = "PositionBasedGeneration", meta = (ToolTip = "Also generate around the registered generation sources (the world map)."))
	bool bUseRegisteredSources = true;

	UPROPERTY()
	TMap<FTile, FTileRecord> GeneratedTiles;

	UPROPERTY(Transient)
	TSet<FTile> PendingTiles;

	UPROPERTY(Transient)
	int CurrentPass = 0;

	FLCOnTilesGenerated OnTilesGenerated;
	FLCOnTilesDestroyed OnTilesDestroyed;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "PositionBasedGeneration")
	void ClearGeneratedTilesCache();

	TArray<UObject*> EvictTiles(const TSet<FTile>& WantedTiles, int NumNewTiles);
};
