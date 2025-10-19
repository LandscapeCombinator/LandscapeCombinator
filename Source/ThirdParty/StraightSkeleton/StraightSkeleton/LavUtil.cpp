#include "LavUtil.h"
#include "CircularList.h"
#include <memory>
/// <summary> Check if two vertex are in the same lav. </summary>

bool LavUtil::IsSameLav(Vertex v1, Vertex v2)
{
    if (v1.List == nullptr || v2.List == nullptr)
        return false;
    return v1.List == v2.List;
}

void LavUtil::RemoveFromLav(std::shared_ptr<Vertex> vertex)
{
    // if removed or not in list, skip
    if (vertex == nullptr || vertex->List == nullptr)
        return;
    vertex->Remove(vertex);
}

/// <summary>
///     Cuts all vertex after given startVertex and before endVertex. start and
///     and vertex are _included_ in cut result.
/// </summary>
/// <param name="startVertex">Start vertex.</param>
/// <param name="endVertex">End vertex.</param>
/// <returns> List of vertex in the middle between start and end vertex. </returns>

std::shared_ptr<std::vector<std::shared_ptr<Vertex>>> LavUtil::CutLavPart(std::shared_ptr<Vertex> startVertex, std::shared_ptr<Vertex> endVertex)
{
    auto ret = std::make_shared<std::vector<std::shared_ptr<Vertex>>>();
    auto size = startVertex->List->Size();
    auto next = startVertex;

    for (size_t i = 0; i < size; i++)
    {
        auto current = next;
        next = std::static_pointer_cast<Vertex>(current->Next);
        current->Remove(current);
        ret->push_back(current);

        if (current == endVertex)
            return ret;
    }

    throw std::runtime_error("End vertex can't be found in start vertex lav");
}

/// <summary>
///     Add all vertex from "merged" lav into "base" lav. Vertex are added before
///     base vertex. Merged vertex order is reversed.
/// </summary>
/// <param name="base">Vertex from lav where vertex will be added.</param>
/// <param name="merged">Vertex from lav where vertex will be removed.</param>

void LavUtil::MergeBeforeBaseVertex(std::shared_ptr<Vertex> base, std::shared_ptr<Vertex> merged)
{
    size_t size = merged->List->Size();
    for (size_t i = 0; i < size; i++)
    {
        auto nextMerged = std::dynamic_pointer_cast<Vertex>(merged->Next);
        nextMerged->Remove(nextMerged);

        base->AddPrevious(base, nextMerged); //TODO verify
       // base->List->AddPrevious(base, nextMerged);
    }
}

/// <summary>
///     Moves all nodes from given vertex lav, to new lav. All moved nodes are
///     added at the end of lav. The lav end is determined by first added vertex
///     to lav.
/// </summary>

void LavUtil::MoveAllVertexToLavEnd(std::shared_ptr<Vertex> vertex, CircularList& newLaw)
{
    size_t size = vertex->List->Size();
    for (size_t i = 0; i < size; i++)
    {
        auto ver = vertex;
        vertex = std::static_pointer_cast<Vertex>(vertex->Next);
        ver->Remove(ver);
        newLaw.AddLast(ver);
    }
}
