// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StraightSkeletonFunctionLibrary.generated.h"

USTRUCT(BlueprintType)
struct FSkeletonEdgeResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    TArray<FVector2D> Polygon;

    UPROPERTY(BlueprintReadWrite)
    FVector2D Begin = FVector2D();

    UPROPERTY(BlueprintReadWrite)
    FVector2D End = FVector2D();
};

USTRUCT(BlueprintType)
struct FStraightSkeleton
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    TArray<FSkeletonEdgeResult> Edges;

    UPROPERTY(BlueprintReadWrite)
    TMap<FVector2D, float> Distances;
};

UCLASS()
class STRAIGHTSKELETONWRAPPER_API UStraightSkeletonFunctionLibrary : public UBlueprintFunctionLibrary
{
public:
    GENERATED_BODY()

    UFUNCTION(BlueprintCallable, Category = "Straight Skeleton")
    static bool ComputeStraightSkeleton(const TArray<FVector2D>& Polygon, FStraightSkeleton& OutSkeleton);
};
