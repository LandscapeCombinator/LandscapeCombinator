#pragma once
#include "Edge.h"
#include "Vertex.h"
#include "EChainType.h"

class IChain
{
private:
	using spe = std::shared_ptr<Edge>;
	using spv = std::shared_ptr<Vertex>;
public:
	virtual EChainType ChainType() { return EChainType::INVALID; };
	virtual ~IChain() {};
	virtual spe PreviousEdge() { return nullptr; };
	virtual spe NextEdge() { return nullptr; };
	virtual spv PreviousVertex() { return nullptr; };
	virtual spv NextVertex() { return nullptr; };
	virtual spv CurrentVertex() { return nullptr; };
};

