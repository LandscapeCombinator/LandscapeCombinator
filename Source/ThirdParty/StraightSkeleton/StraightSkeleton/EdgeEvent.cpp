#include "EdgeEvent.h"

bool EdgeEvent::IsObsolete()
{
    return (PreviousVertex->IsProcessed || NextVertex->IsProcessed);
}

EdgeEvent::EdgeEvent(spv2d point, double distance, spv previousVertex, spv nextVertex) : SkeletonEvent(point, distance)
{
    PreviousVertex = previousVertex;
    NextVertex = nextVertex;
}

EdgeEvent::~EdgeEvent()
{
    NextVertex = nullptr;
    PreviousVertex = nullptr;
}

std::string EdgeEvent::ToString()
{
    std::string str1 = V->ToString();
    std::string str2 = (PreviousVertex != nullptr ? PreviousVertex->Point->ToString() : "null");
    std::string str3 = (NextVertex != nullptr ? NextVertex->Point->ToString() : "null");
    return std::string("EdgeEvent " + str1 + " PreviousVertex = " + str2 +" NextVertex = " + str3 + " Distance = " + std::to_string(SkeletonEvent::Distance));
}
