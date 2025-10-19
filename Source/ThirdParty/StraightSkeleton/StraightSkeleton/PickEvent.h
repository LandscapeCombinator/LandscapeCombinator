#pragma once
#include "SkeletonEvent.h"
#include "EdgeChain.h"
class PickEvent :
    public SkeletonEvent
{
private:
    using spv2d = std::shared_ptr<Vector2d>;
    using spec = std::shared_ptr<EdgeChain>;
public:
    spec Chain = nullptr;
    PickEvent(spv2d point, double distance, spec chain);
    virtual ~PickEvent();
    bool IsObsolete() override;
};
