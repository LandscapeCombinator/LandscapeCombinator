// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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

UCLASS(BlueprintType)
class LCCOMMON_API ULCPositionBasedGeneration : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "PositionBasedGeneration", meta=(DisplayPriority=1))
    bool bEnablePositionBasedGeneration = false;

    UPROPERTY(EditAnywhere, Category = "PositionBasedGeneration", meta=(DisplayPriority=4))
    int ZoomLevel = 14;

    UPROPERTY(EditAnywhere, Category = "PositionBasedGeneration", meta=(DisplayPriority=5))
    // Distance 0 means 1 tile, Distance 1 means 3x3=9 tiles, Distance 2 means 5x5=25 tiles, etc.
    int GenerateAllTilesAtDistance = 1;

    TSet<FTile> GeneratedTiles;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "PositionBasedGeneration")
    void ClearGeneratedTilesCache();
};
