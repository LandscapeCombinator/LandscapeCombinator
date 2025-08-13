#pragma once
#include <vector>
#include <map>
#include "EdgeResult.h"
#include "Vector2d.h"

class STRAIGHTSKELETON_API Skeleton
{
private:
	using spver = std::shared_ptr<std::vector<std::shared_ptr<EdgeResult>>>;
	using spmv2d = std::shared_ptr<std::map<Vector2d, double>>;
public:
	spver Edges = nullptr;
	spmv2d Distances = nullptr;
	Skeleton(spver edges, spmv2d distances);
	~Skeleton();
};

