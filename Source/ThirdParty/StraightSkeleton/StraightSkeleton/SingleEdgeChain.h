#pragma once
#include "IChain.h"
#include "CircularList.h"
#include "EChainType.h"

class SingleEdgeChain :
    public IChain
{
private:
    using spv = std::shared_ptr<Vertex>;
    using spe = std::shared_ptr<Edge>;
    spv _nextVertex = nullptr;
    spe _oppositeEdge = nullptr;
    spv _previousVertex = nullptr;
public:
    SingleEdgeChain(spe oppositeEdge, spv nextVertex);
    ~SingleEdgeChain() override;
    spe PreviousEdge() override;
    spe NextEdge() override;
    spv PreviousVertex() override;
    spv NextVertex() override;
    spv CurrentVertex() override;
    EChainType ChainType() override;
};

