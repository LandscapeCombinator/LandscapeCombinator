#include "SplitChain.h"
#include <memory>

SplitChain::SplitChain(spse event)
{
    _splitEvent = event;
}

SplitChain::~SplitChain()
{
    _splitEvent = nullptr;
}

std::shared_ptr<Edge> SplitChain::OppositeEdge()
{
    if (typeid(*_splitEvent) != typeid(VertexSplitEvent))
        return _splitEvent->OppositeEdge;
    return nullptr;
}

std::shared_ptr<Edge> SplitChain::PreviousEdge()
{
    return _splitEvent->Parent->PreviousEdge;
}

std::shared_ptr<Edge> SplitChain::NextEdge()
{
    return _splitEvent->Parent->NextEdge;
}

std::shared_ptr<Vertex> SplitChain::PreviousVertex()
{
    return std::dynamic_pointer_cast<Vertex>(_splitEvent->Parent->Previous);
}

std::shared_ptr<Vertex> SplitChain::NextVertex()
{
    return std::dynamic_pointer_cast<Vertex>(_splitEvent->Parent->Next);
}

std::shared_ptr<Vertex> SplitChain::CurrentVertex()
{
    return _splitEvent->Parent;
}

EChainType SplitChain::ChainType()
{
    return EChainType::SPLIT;
}
