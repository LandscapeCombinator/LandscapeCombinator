#include "EdgeChain.h"

EdgeChain::EdgeChain(spvee edgeList)
{
    EdgeList = edgeList;
    _closed = PreviousVertex() == NextVertex();
}

EdgeChain::~EdgeChain()
{
    EdgeList = nullptr;
}

std::shared_ptr<Edge> EdgeChain::PreviousEdge()
{
    return EdgeList->at(0)->PreviousVertex->PreviousEdge;
}

std::shared_ptr<Edge> EdgeChain::NextEdge()
{
    return EdgeList->at(EdgeList->size() - 1)->NextVertex->NextEdge;
}

std::shared_ptr<Vertex> EdgeChain::PreviousVertex()
{
    return EdgeList->at(0)->PreviousVertex;
}

std::shared_ptr<Vertex> EdgeChain::NextVertex()
{
    return EdgeList->at(EdgeList->size() - 1)->NextVertex;
}

std::shared_ptr<Vertex> EdgeChain::CurrentVertex()
{
    return nullptr;
}

EChainType EdgeChain::ChainType()
{
    return (_closed ? EChainType::CLOSED_EDGE : EChainType::EDGE);
}
