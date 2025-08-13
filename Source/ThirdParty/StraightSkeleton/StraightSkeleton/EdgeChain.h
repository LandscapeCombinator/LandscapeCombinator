#pragma once
#include <vector>
#include "IChain.h"
#include "EdgeEvent.h"
#include "EChainType.h"

class EdgeChain :
    public IChain
{
private:
    using spe = std::shared_ptr<Edge>;
    using spv = std::shared_ptr<Vertex>;
    using spvee = std::shared_ptr<std::vector<std::shared_ptr<EdgeEvent>>>;
    bool _closed = false;
public:
    spvee EdgeList = nullptr;
    EdgeChain(spvee edgeList);
    ~EdgeChain() override;
    spe PreviousEdge() override;
    spe NextEdge() override;
    spv PreviousVertex() override;
    spv NextVertex() override;
    spv CurrentVertex() override;
    EChainType ChainType() override;
};

