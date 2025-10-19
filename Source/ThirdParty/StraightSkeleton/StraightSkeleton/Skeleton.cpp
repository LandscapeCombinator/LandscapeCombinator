#include "Skeleton.h"

Skeleton::Skeleton(spver edges, spmv2d distances)
{
	Edges = edges;
	Distances = distances;
}

Skeleton::~Skeleton()
{
	Edges = nullptr;
	Distances = nullptr;
}
