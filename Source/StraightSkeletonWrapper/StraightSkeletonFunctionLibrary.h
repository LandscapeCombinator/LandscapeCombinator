// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StraightSkeletonFunctionLibrary.generated.h"

USTRUCT(BlueprintType)
struct FSkeletonEdgeResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Skeleton Edge Result")
    TArray<FVector2D> Polygon;

    UPROPERTY(BlueprintReadWrite, Category = "Skeleton Edge Result")
    FVector2D Begin = FVector2D();

    UPROPERTY(BlueprintReadWrite, Category = "Skeleton Edge Result")
    FVector2D End = FVector2D();
};

USTRUCT(BlueprintType)
struct FStraightSkeleton
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Straight Skeleton")
    TArray<FSkeletonEdgeResult> Edges;

    UPROPERTY(BlueprintReadWrite, Category = "Straight Skeleton")
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
