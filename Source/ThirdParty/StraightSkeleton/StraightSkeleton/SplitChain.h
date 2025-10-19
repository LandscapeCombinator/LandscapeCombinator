#pragma once
#include <typeinfo>
#include "IChain.h"
#include "SplitEvent.h"
#include "VertexSplitEvent.h"

class SplitChain :
    public IChain
{    
private:
    using spe = std::shared_ptr<Edge>;
    using spv = std::shared_ptr<Vertex>;
    using spse = std::shared_ptr<SplitEvent>;
    spse _splitEvent = nullptr;
public:
    SplitChain(spse event);
    ~SplitChain() override;
    spe OppositeEdge();
    spe PreviousEdge() override;
    spe NextEdge() override;
    spv PreviousVertex() override;
    spv NextVertex() override;
    spv CurrentVertex() override;
    EChainType ChainType() override;
};

