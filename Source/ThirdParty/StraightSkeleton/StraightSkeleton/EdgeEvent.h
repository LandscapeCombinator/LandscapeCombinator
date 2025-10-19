#pragma once
#include <string>
#include "SkeletonEvent.h"
#include "Vertex.h"
#include "Vector2d.h"

class EdgeEvent :
    public SkeletonEvent
{
private:
    using spv2d = std::shared_ptr<Vector2d>;
    using spv = std::shared_ptr<Vertex>;
public:
    spv NextVertex = nullptr;
    spv PreviousVertex = nullptr;
    bool IsObsolete() override;
    EdgeEvent(spv2d point, double distance, spv previousVertex, spv nextVertex);
    virtual ~EdgeEvent();
    std::string ToString();
};

