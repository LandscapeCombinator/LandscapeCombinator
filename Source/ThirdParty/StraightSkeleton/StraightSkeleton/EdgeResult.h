#pragma once
#include <vector>
#include "Edge.h"
#include "Vector2d.h"
class EdgeResult
{
private:
	using spe = std::shared_ptr<Edge>;
	using spvv2d = std::shared_ptr<std::vector<std::shared_ptr<Vector2d>>>;
public:
	spe edge = nullptr;
	spvv2d Polygon = nullptr;
    EdgeResult(spe edge, spvv2d polygon);
	~EdgeResult();
};

