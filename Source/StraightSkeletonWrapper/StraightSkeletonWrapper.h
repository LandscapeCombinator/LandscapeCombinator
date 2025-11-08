// Copyright 2023-2025 LandscapeCombinator. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StraightSkeletonWrapper.generated.h"

USTRUCT()
struct FSkeletonEdgeResult
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FVector2D> Polygon;

    UPROPERTY()
    FVector2D Begin = FVector2D();

    UPROPERTY()
    FVector2D End = FVector2D();
};

USTRUCT()
struct FStraightSkeleton
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FSkeletonEdgeResult> Edges;

    UPROPERTY()
    TMap<FVector2D, float> Distances;
};

class STRAIGHTSKELETONWRAPPER_API StraightSkeletonWrapper
{
public:
    static bool ComputeStraightSkeleton(const TArray<FVector2D>& Polygon, FStraightSkeleton& OutSkeleton); 
};
