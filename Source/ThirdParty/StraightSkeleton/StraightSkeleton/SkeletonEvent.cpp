#include "SkeletonEvent.h"

SkeletonEvent::SkeletonEvent(spv2d point, double distance)
{
    V = point;
    Distance = distance;
}

SkeletonEvent::~SkeletonEvent()
{
    V = nullptr;
}

std::string SkeletonEvent::ToString()
{
    return std::string("IntersectEntry " + V->ToString() + ", Distance = " + std::to_string(Distance));
}
