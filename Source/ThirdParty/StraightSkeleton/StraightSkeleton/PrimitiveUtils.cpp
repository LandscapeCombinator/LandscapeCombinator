#include "PrimitiveUtils.h"

PrimitiveUtils::IntersectPoints PrimitiveUtils::Empty = IntersectPoints();

PrimitiveUtils::IntersectPoints::IntersectPoints()
{
    Intersect = Vector2d::Empty();
    IntersectEnd = Vector2d::Empty();
}

PrimitiveUtils::IntersectPoints::IntersectPoints(Vector2d intersect, Vector2d intersectEnd)
{
    Intersect = intersect;
    IntersectEnd = intersectEnd;
}

PrimitiveUtils::IntersectPoints::IntersectPoints(Vector2d intersect)
{
    Intersect = intersect;
    IntersectEnd = Vector2d::Empty();
}

Vector2d PrimitiveUtils::FromTo(Vector2d begin, Vector2d end)
{
    return Vector2d(end.X - begin.X, end.Y - begin.Y);
}

Vector2d PrimitiveUtils::OrthogonalLeft(Vector2d v)
{
    return Vector2d(-v.Y, v.X);
}

Vector2d PrimitiveUtils::OrthogonalRight(Vector2d v)
{
    return Vector2d(v.Y, -v.X);
}

Vector2d PrimitiveUtils::OrthogonalProjection(Vector2d unitVector, Vector2d vectorToProject)
{
    Vector2d n = unitVector.Normalized();

    double px = vectorToProject.X;
    double py = vectorToProject.Y;

    double ax = n.X;
    double ay = n.Y;

    return Vector2d(px * ax * ax + py * ax * ay, px * ax * ay + py * ay * ay);
}

Vector2d PrimitiveUtils::BisectorNormalized(Vector2d norm1, Vector2d norm2)
{
    Vector2d e1v = OrthogonalLeft(norm1);
    Vector2d e2v = OrthogonalLeft(norm2);

    // 90 - 180 || 180 - 270
    if (norm1.Dot(norm2) > 0)
        return e1v + e2v;

    // 0 - 180
    Vector2d ret = Vector2d(norm1);
    ret.Negate();
    ret += norm2;

    // 270 - 360
    if (e1v.Dot(norm2) < 0)
        ret.Negate();

    return ret;
}

bool PrimitiveUtils::IsPointOnRay(Vector2d point, LineParametric2d ray, double epsilon)
{
    Vector2d rayDirection = ray.U->Normalized();
    // test if point is on ray
    Vector2d pointVector = point - *ray.A;

    double dot = rayDirection.Dot(pointVector);

    if (dot < epsilon)
        return false;

    double x = rayDirection.X;
    rayDirection.X = rayDirection.Y;
    rayDirection.Y = -x;

    dot = rayDirection.Dot(pointVector);

    return -epsilon < dot&& dot < epsilon;
}

bool PrimitiveUtils::InCollinearRay(Vector2d p, Vector2d rayStart, Vector2d rayDirection)
{
    // test if point is on ray
    Vector2d collideVector = p - rayStart;
    double dot = rayDirection.Dot(collideVector);

    return !(dot < 0);
}

double PrimitiveUtils::Dot(Vector2d u, Vector2d v)
{
    return u.Dot(v);
}

/// <summary> Perp Dot Product. </summary>
double PrimitiveUtils::Perp(Vector2d u, Vector2d v)
{
    return u.X * v.Y - u.Y * v.X;
}

/// <summary>
///     Calculate intersection points for rays. It can return more then one
///     intersection point when rays overlaps.
///     <see href="http://geomalgorithms.com/a05-_intersect-1.html" />
///     <see href="http://softsurfer.com/Archive/algorithm_0102/algorithm_0102.htm" />
/// </summary>
/// <returns>class with intersection points. It never return null.</returns>

PrimitiveUtils::IntersectPoints PrimitiveUtils::IntersectRays2D(LineParametric2d r1, LineParametric2d r2)
{
    Vector2d s1p0 = *r1.A;
    Vector2d s1p1 = *r1.A + *r1.U;

    Vector2d s2p0 = *r2.A;

    Vector2d u = *r1.U;
    Vector2d v = *r2.U;

    Vector2d w = s1p0 - s2p0;
    double d = Perp(u, v);

    // test if they are parallel (includes either being a point)
    if (fabs(d) < SmallNum)
    {
        // they are NOT collinear
        // S1 and S2 are parallel
        if (Perp(u, w) != 0 || Perp(v, w) != 0)
            return Empty;

        // they are collinear or degenerate
        // check if they are degenerate points
        double du = Dot(u, u);
        double dv = Dot(v, v);
        if (du == 0 && dv == 0)
        {
            // both segments are points
            if (s1p0 != s2p0)
                return Empty;

            // they are the same point
            return IntersectPoints(s1p0);
        }
        if (du == 0)
        {
            // S1 is a single point
            if (!InCollinearRay(s1p0, s2p0, v))
                return Empty;

            return IntersectPoints(s1p0);
        }
        if (dv == 0)
        {
            // S2 a single point
            if (!InCollinearRay(s2p0, s1p0, u))
                return Empty;

            return IntersectPoints(s2p0);
        }
        // they are collinear segments - get overlap (or not)
        double t0, t1;
        // endpoints of S1 in eqn for S2
        Vector2d w2 = s1p1 - s2p0;
        if (v.X != 0)
        {
            t0 = w.X / v.X;
            t1 = w2.X / v.X;
        }
        else
        {
            t0 = w.Y / v.Y;
            t1 = w2.Y / v.Y;
        }
        if (t0 > t1)
        {
            // must have t0 smaller than t1
            double t = t0;
            t0 = t1;
            t1 = t; // swap if not
        }
        if (t1 < 0)
            // NO overlap
            return Empty;

        // clip to min 0
        t0 = t0 < 0 ? 0 : t0;

        if (t0 == t1)
        {
            // intersect is a point
            Vector2d I0 = Vector2d(v);
            I0 *= t0;
            I0 += s2p0;

            return IntersectPoints(I0);
        }

        // they overlap in a valid subsegment

        // I0 = S2_P0 + t0 * v;
        Vector2d I_0 = Vector2d(v);
        I_0 *= t0;
        I_0 += s2p0;

        // I1 = S2_P0 + t1 * v;
        Vector2d I1 = Vector2d(v);
        I1 *= t1;
        I1 += s2p0;

        return IntersectPoints(I_0, I1);
    }

    // the segments are skew and may intersect in a point
    // get the intersect parameter for S1
    double sI = Perp(v, w) / d;
    if (sI < 0 /* || sI > 1 */)
        return Empty;

    // get the intersect parameter for S2
    double tI = Perp(u, w) / d;
    if (tI < 0 /* || tI > 1 */)
        return Empty;

    // I0 = S1_P0 + sI * u; // compute S1 intersect point
    Vector2d IO = Vector2d(u);
    IO *= sI;
    IO += s1p0;

    return IntersectPoints(IO);
}

/// <summary>
///     Test if point is inside polygon.
///     <see href="http://en.wikipedia.org/wiki/Point_in_polygon" />
///     <see href="http://en.wikipedia.org/wiki/Even-odd_rule" />
///     <see href="http://paulbourke.net/geometry/insidepoly/" />
/// </summary>

bool PrimitiveUtils::IsPointInsidePolygon(Vector2d point, std::shared_ptr<std::vector<std::shared_ptr<Vector2d>>> points)
{
    size_t numpoints = points->size();

    if (numpoints < 3)
        return false;

    size_t it = 0;
    std::shared_ptr<Vector2d> first = points->at(it);
    bool oddNodes = false;

    for (size_t i = 0; i < numpoints; i++)
    {
        std::shared_ptr<Vector2d> node1 = points->at(it);
        it++;
        std::shared_ptr<Vector2d> node2 = i == numpoints - 1 ? first : points->at(it);

        double x = point.X;
        double y = point.Y;

        if (node1->Y < y && node2->Y >= y || node2->Y < y && node1->Y >= y)
        {
            if (node1->X + (y - node1->Y) / (node2->Y - node1->Y) * (node2->X - node1->X) < x)
                oddNodes = !oddNodes;
        }
    }

    return oddNodes;
}

/// <summary> Calculate area of polygon outline. For clockwise are will be less then. </summary>
/// <param name="polygon">List of polygon points.</param>
/// <returns> Area. </returns>

double PrimitiveUtils::Area(std::vector<Vector2d>& polygon)
{
    size_t n = polygon.size();
    double A = 0.0f;
    for (size_t p = n - 1, q = 0; q < n; p = q++)
        A += polygon[p].X * polygon[q].Y - polygon[q].X * polygon[p].Y;

    return A * 0.5f;
}

/// <summary> Check if polygon is clockwise. </summary>
/// <param name="polygon"> List of polygon points. </param>
/// <returns> If polygon is clockwise. </returns>

bool PrimitiveUtils::IsClockwisePolygon(std::vector<Vector2d>& polygon)
{
    return Area(polygon) < 0;
}

std::vector<Vector2d> PrimitiveUtils::MakeCounterClockwise(std::vector<Vector2d>& polygon)
{
    if (IsClockwisePolygon(polygon))
        std::reverse(polygon.begin(), polygon.end());
    return polygon;
}
