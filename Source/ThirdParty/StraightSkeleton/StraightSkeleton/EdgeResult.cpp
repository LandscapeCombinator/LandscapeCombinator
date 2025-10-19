#include "EdgeResult.h"

EdgeResult::EdgeResult(spe edge, spvv2d polygon)
{
    this->edge = edge;
    Polygon = polygon;
}

EdgeResult::~EdgeResult()
{
    edge = nullptr;
    Polygon = nullptr;
}
