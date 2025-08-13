#pragma once
#include "Vector2d.h"
#include "LineParametric2d.h"
#include <vector>
#include <algorithm> 

class PrimitiveUtils
{
public:
    struct IntersectPoints
    {
    public:
        Vector2d Intersect;
        Vector2d IntersectEnd;
        IntersectPoints();
        IntersectPoints(Vector2d intersect, Vector2d intersectEnd);
        IntersectPoints(Vector2d intersect);
    };
    static Vector2d FromTo(Vector2d begin, Vector2d end);
    static Vector2d OrthogonalLeft(Vector2d v);
    static Vector2d OrthogonalRight(Vector2d v);
    static Vector2d OrthogonalProjection(Vector2d unitVector, Vector2d vectorToProject);
    static Vector2d BisectorNormalized(Vector2d norm1, Vector2d norm2);
    static bool IsPointOnRay(Vector2d point, LineParametric2d ray, double epsilon);
    static bool InCollinearRay(Vector2d p, Vector2d rayStart, Vector2d rayDirection);
    static double Dot(Vector2d u, Vector2d v);
    static double Perp(Vector2d u, Vector2d v);
    static IntersectPoints IntersectRays2D(LineParametric2d r1, LineParametric2d r2);
    static bool IsPointInsidePolygon(Vector2d point, std::shared_ptr<std::vector<std::shared_ptr<Vector2d>>> points);
    static double Area(std::vector<Vector2d>& polygon);
    static bool IsClockwisePolygon(std::vector<Vector2d>& polygon);
    static std::vector<Vector2d> MakeCounterClockwise(std::vector<Vector2d>& polygon);
    
private:
    /// <summary> Error epsilon. Anything that avoids division. </summary>
    static constexpr double SmallNum = 0.00000001;
    /// <summary> Return value if there is no intersection. </summary>
    static IntersectPoints Empty;
};

