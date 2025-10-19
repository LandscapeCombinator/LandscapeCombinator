#pragma once
#include <string>
#include "SkeletonEvent.h"
#include "Edge.h"
#include "Vertex.h"

class SplitEvent :
    public SkeletonEvent
{
private:
    using spv2d = std::shared_ptr<Vector2d>;
    using spe = std::shared_ptr<Edge>;
    using spv = std::shared_ptr<Vertex>;
public:
    spe OppositeEdge = nullptr;
    spv Parent = nullptr;
    SplitEvent(spv2d point, double distance, spv parent, spe oppositeEdge);
    virtual ~SplitEvent();
    bool IsObsolete() override;
    std::string ToString();
};


