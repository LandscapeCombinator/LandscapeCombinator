#include "Vertex.h"
#include "CircularList.h"

Vertex::Vertex()
{
    Point = nullptr;
    Bisector = nullptr;
    NextEdge = nullptr;
    PreviousEdge = nullptr;
    Distance = std::numeric_limits<double>::quiet_NaN();
    IsProcessed = false;
    LeftFace = nullptr;
    RightFace = nullptr;
}

Vertex::Vertex(spv2d point, double distance, splp2d bisector, spe previousEdge, spe nextEdge)
{
    Point = point;
    Bisector = bisector;
    NextEdge = nextEdge;
    PreviousEdge = previousEdge;
    Distance = distance;
    IsProcessed = false;
    LeftFace = nullptr;
    RightFace = nullptr;
}

Vertex& Vertex::operator = (const Vertex& other)
{
    if (this != &other)
    {
        Point = other.Point;
        Distance = other.Distance;
        Bisector = other.Bisector;
        PreviousEdge = other.PreviousEdge;
        NextEdge = other.NextEdge;
        IsProcessed = other.IsProcessed;
        LeftFace = other.LeftFace;
        RightFace = other.RightFace;
    }
    return *this;
}

std::string Vertex::ToString() const
{
    return std::string("Vertex [v="+ Point->ToString() + 
      ", IsProcessed="+std::to_string(IsProcessed) + 
      ", Bisector= "+ Bisector->ToString() + 
      ", PreviousEdge="+ PreviousEdge->ToString() +", NextEdge="+ NextEdge->ToString() +"]");
}

double Vertex::Round(double value, double precision)
{
    double p10 = pow(10.0f, precision);
    return round(value * p10) / p10;
}

Vertex::~Vertex()
{
    Point = nullptr;
    Bisector = nullptr;
    NextEdge = nullptr;
    PreviousEdge = nullptr;
    Distance = std::numeric_limits<double>::quiet_NaN();
    IsProcessed = false;
    LeftFace = nullptr;
    RightFace = nullptr;
}