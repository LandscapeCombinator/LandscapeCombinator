#include "StraightSkeletonWrapper.h"
// #include "../ThirdParty/StraightSkeleton/StraightSkeleton/Vector2d.h"
#include "../ThirdParty/StraightSkeleton/StraightSkeleton/SkeletonBuilder.h"
// #include "StraightSkeletonBuilder.h"
#include <vector>
#include <map>

bool StraightSkeletonWrapper::ComputeStraightSkeleton(const TArray<FVector2D>& Polygon, FStraightSkeleton& OutSkeleton)
{
    std::vector<Vector2d> RawPolygon;
    for (const FVector2D& V : Polygon)
        RawPolygon.emplace_back(V.X, V.Y);

    try {
        Skeleton Skel = SkeletonBuilder::Build(RawPolygon);

        if (!Skel.Edges || !Skel.Distances) return false;

        OutSkeleton.Edges.Empty();
        for (auto& ER : *(Skel.Edges))
        {
            if (!ER || !ER->edge || !ER->Polygon) return false;

            FSkeletonEdgeResult EdgeResult;
            EdgeResult.Begin = FVector2D(ER->edge->Begin->X, ER->edge->Begin->Y);
            EdgeResult.End = FVector2D(ER->edge->End->X, ER->edge->End->Y);

            for (auto& P : *(ER->Polygon))
                EdgeResult.Polygon.Add(FVector2D(P->X, P->Y));

            OutSkeleton.Edges.Add(EdgeResult);
        }

        OutSkeleton.Distances.Empty();
        for (auto& Pair : *(Skel.Distances))
            OutSkeleton.Distances.Add(FVector2D(Pair.first.X, Pair.first.Y), static_cast<float>(Pair.second));

        return true;
    }
    catch (...) {
        UE_LOG(LogTemp, Error, TEXT("StraightSkeleton library threw an exception on Polygon"));
        for (auto &P: Polygon)
        {
            UE_LOG(LogTemp, Error, TEXT("%s"), *P.ToString());
        }
        return false;
    }
}
