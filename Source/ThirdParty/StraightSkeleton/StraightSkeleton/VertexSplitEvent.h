#pragma once
#include <string>
#include "SplitEvent.h"

class VertexSplitEvent :
    public SplitEvent
{
private:
    using spv2d = std::shared_ptr<Vector2d>;
    using spv = std::shared_ptr<Vertex>;
public:
    VertexSplitEvent(spv2d point, double distance, spv parent);
    std::string ToString();
};

