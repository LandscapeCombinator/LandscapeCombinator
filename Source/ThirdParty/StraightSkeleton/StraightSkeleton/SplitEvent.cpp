#include "SplitEvent.h"

SplitEvent::SplitEvent(spv2d point, double distance, spv parent, spe oppositeEdge)
    : SkeletonEvent(point, distance)
{
    Parent = parent;
    OppositeEdge = (oppositeEdge != nullptr ? oppositeEdge : nullptr);
}

SplitEvent::~SplitEvent()
{
    OppositeEdge = nullptr;
    Parent = nullptr;
}

bool SplitEvent::IsObsolete()
{
    return Parent->IsProcessed;
}

std::string SplitEvent::ToString()
{
    return std::string("SplitEvent " + V->ToString() + " Parent = " + (Parent != nullptr ? Parent->Point->ToString() : "null") + " Distance = " + std::to_string(Distance));
}
