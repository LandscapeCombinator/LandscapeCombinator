#pragma once
#include <string>
#include <memory>
#include "Vector2d.h"

class SkeletonEvent
{
private:
    using spv2d = std::shared_ptr<Vector2d>;
public:
    struct SkeletonEventComparer
    {
        bool operator()(SkeletonEvent& left, SkeletonEvent& right)
        {
            return left.Distance < right.Distance;
        }
    };
    spv2d V = nullptr;
    double Distance;    
    SkeletonEvent(spv2d point, double distance);
    virtual ~SkeletonEvent();
    std::string ToString();
    virtual bool IsObsolete() { return false; };
};

