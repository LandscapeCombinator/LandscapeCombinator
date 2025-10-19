#include "SingleEdgeChain.h"
#include <memory>
SingleEdgeChain::SingleEdgeChain(spe oppositeEdge, spv nextVertex)
{
    _oppositeEdge = oppositeEdge;
    _nextVertex = nextVertex;
    _previousVertex = std::dynamic_pointer_cast<Vertex>(nextVertex->Previous);
}
SingleEdgeChain::~SingleEdgeChain()
{
    _nextVertex = nullptr;
    _oppositeEdge = nullptr;
    _previousVertex = nullptr;
}

std::shared_ptr<Edge> SingleEdgeChain::PreviousEdge()
{
    return _oppositeEdge;
}

std::shared_ptr<Edge> SingleEdgeChain::NextEdge()
{
    return _oppositeEdge;
}

std::shared_ptr<Vertex> SingleEdgeChain::PreviousVertex()
{
    return _previousVertex;
}

std::shared_ptr<Vertex> SingleEdgeChain::NextVertex()
{
    return _nextVertex;
}

std::shared_ptr<Vertex> SingleEdgeChain::CurrentVertex()
{
    return nullptr;
}

EChainType SingleEdgeChain::ChainType()
{
    return EChainType::SPLIT;
}
