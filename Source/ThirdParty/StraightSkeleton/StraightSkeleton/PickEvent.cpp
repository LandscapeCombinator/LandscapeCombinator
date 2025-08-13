#include "PickEvent.h"

PickEvent::PickEvent(spv2d point, double distance, spec chain) : SkeletonEvent(point, distance)
{
    Chain = chain;
}

PickEvent::~PickEvent()
{
    Chain = nullptr;
}

bool PickEvent::IsObsolete()
{
    return false;
}
