#include "VertexSplitEvent.h"

VertexSplitEvent::VertexSplitEvent(spv2d point, double distance, spv parent) :
    SplitEvent(point, distance, parent, nullptr)
{
}

std::string VertexSplitEvent::ToString()
{
    return std::string("VertexSplitEvent [V=" + V->ToString() + " , Parent = " + (Parent != nullptr ? Parent->Point->ToString() : "null") + " , Distance = " + std::to_string( Distance ) );
}
