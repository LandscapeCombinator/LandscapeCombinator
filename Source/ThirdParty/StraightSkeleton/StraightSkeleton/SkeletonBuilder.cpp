#include "SkeletonBuilder.h"
#include "CircularList.h"
#include <float.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpotentially-evaluated-expression"
#endif

// const double SkeletonBuilder::SplitEpsilon = 1E-10;
const double SkeletonBuilder::SplitEpsilon = 5E-6;

Skeleton SkeletonBuilder::Build(listVector2d& polygon)
{
	std::vector<std::vector<Vector2d>> holes = std::vector<std::vector<Vector2d>>();
	return Build(polygon, holes);
}

Skeleton SkeletonBuilder::Build(listVector2d& polygon, nestedlistVector2d& holes)
{
	InitPolygon(polygon);
	MakeClockwise(holes);

	auto queue = std::make_shared<queueSkeletonEvent>(); 
	auto sLav = std::make_shared<hashsetCircularList>();
	auto faces = std::make_shared<listFaceQueue>();
	auto edges = std::make_shared<listEdge>();

	InitSlav(polygon, sLav, edges, faces);

	if (!holes.empty())
	{
		for (auto inner : holes)
			InitSlav(inner, sLav, edges, faces);
	}

	InitEvents(sLav, queue, edges);

	auto count = 0;
	while (!queue->empty())
	{
		// start processing skeleton level
		count = AssertMaxNumberOfInteraction(count);
		auto levelHeight = queue->top()->Distance;
		auto eventQueue = LoadAndGroupLevelEvents(queue);
		for (auto event : *eventQueue)
		{
			// event is outdated some of parent vertex was processed before
			if (event->IsObsolete())
				continue;

			if (typeid(*event) == typeid(EdgeEvent))
				throw std::runtime_error("All edge@events should be converted to MultiEdgeEvents for given level");
			if (typeid(*event) == typeid(SplitEvent))
				throw std::runtime_error("All split events should be converted to MultiSplitEvents for given level");
			if (typeid(*event) == typeid(MultiSplitEvent))
				EmitMultiSplitEvent(std::dynamic_pointer_cast<MultiSplitEvent>(event), sLav, queue, edges);
			else if (typeid(*event) == typeid(PickEvent))
				EmitPickEvent(std::dynamic_pointer_cast<PickEvent>(event));
			else if (typeid(*event) == typeid(MultiEdgeEvent))
				EmitMultiEdgeEvent(std::dynamic_pointer_cast<MultiEdgeEvent>(event), queue, edges);
			else
				throw std::runtime_error(std::string("Unknown event type: ") + std::string(typeid(*event).name()));

		}

		ProcessTwoNodeLavs(sLav);
		RemoveEventsUnderHeight(queue, levelHeight);
		RemoveEmptyLav(sLav);
	}

	return AddFacesToOutput(faces);

}

std::vector<Vector2d> SkeletonBuilder::InitPolygon(std::vector<Vector2d>& polygon)
{
	if (polygon.size() == 0)
		throw std::runtime_error("polygon can't be null");
	if (polygon.at(0).Equals(polygon.at(polygon.size() - 1)))
		throw std::runtime_error("polygon can't start and end with the same point");

	return MakeCounterClockwise(polygon);
}

void SkeletonBuilder::ProcessTwoNodeLavs(sp<hashsetCircularList> sLav)
{
	for (auto lav : *sLav)
	{
		if (lav->Size() == 2)
		{
			auto first = std::static_pointer_cast<Vertex>(lav->First());
			auto last = std::static_pointer_cast<Vertex>(first->Next);

			FaceQueueUtil::ConnectQueues(first->LeftFace, last->RightFace);
			FaceQueueUtil::ConnectQueues(first->RightFace, last->LeftFace);

			first->IsProcessed = true;
			last->IsProcessed = true;

			LavUtil::RemoveFromLav(first);
			LavUtil::RemoveFromLav(last);
		}
	}
}

void SkeletonBuilder::RemoveEmptyLav(sp<hashsetCircularList> sLav)
{
reset:
	for (auto list : *sLav)
	{
		if (list->Size() == 0)
		{
			sLav->erase(list);
			goto reset;
		}
	}
}

void SkeletonBuilder::EmitMultiEdgeEvent(sp<MultiEdgeEvent> event, sp<queueSkeletonEvent> queue, sp<listEdge> edges)
{
	auto center = event->V;
	auto edgeList = event->Chain->EdgeList;

	auto previousVertex = event->Chain->PreviousVertex();
	previousVertex->IsProcessed = true;

	auto nextVertex = event->Chain->NextVertex();
	nextVertex->IsProcessed = true;

	auto bisector = CalcBisector(center, *previousVertex->PreviousEdge, *nextVertex->NextEdge);
	auto edgeVertex = std::make_shared<Vertex>(center, event->Distance, bisector, previousVertex->PreviousEdge, nextVertex->NextEdge);

	// left face
	AddFaceLeft(edgeVertex, previousVertex);
	// right face
	AddFaceRight(edgeVertex, nextVertex);

	previousVertex->AddPrevious(previousVertex, edgeVertex);

	// back faces
	AddMultiBackFaces(edgeList, edgeVertex);

	ComputeEvents(edgeVertex, queue, edges);
}

void SkeletonBuilder::AddMultiBackFaces(sp<listEdgeEvent> edgeList, sp<Vertex> edgeVertex)
{
	for (auto edgeEvent : *edgeList)
	{
		edgeEvent->PreviousVertex->IsProcessed = true;
		LavUtil::RemoveFromLav(edgeEvent->PreviousVertex);

		edgeEvent->NextVertex->IsProcessed = true;
		LavUtil::RemoveFromLav(edgeEvent->NextVertex);

		AddFaceBack(edgeVertex, edgeEvent->PreviousVertex, edgeEvent->NextVertex);
	}
}

void SkeletonBuilder::EmitPickEvent(sp<PickEvent> event)
{
	auto center = event->V;
	auto edgeList = event->Chain->EdgeList;

	// lav will be removed so it is final vertex.
	auto newVertex = std::make_shared<Vertex>(center, event->Distance, std::make_shared<LineParametric2d>(), nullptr, nullptr);
	newVertex->IsProcessed = true;
	AddMultiBackFaces(edgeList, newVertex);
}

void SkeletonBuilder::EmitMultiSplitEvent(sp<MultiSplitEvent> event, sp<hashsetCircularList> sLav, sp<queueSkeletonEvent> queue, sp<listEdge> edges)
{
	auto chains = event->Chains;
	auto center = event->V;
	CreateOppositeEdgeChains(sLav, chains, center);
	
	std::sort(chains->begin(), chains->end(), SkeletonBuilder::ChainComparer(*center));

	// face node for split@event is shared between two chains
	std::shared_ptr<FaceNode> lastFaceNode = nullptr;

	// connect all edges into new bisectors and lavs
	auto edgeListSize = chains->size();
	for (size_t i = 0; i < edgeListSize; i++)
	{
		auto chainBegin = chains->at(i);
		auto chainEnd = chains->at((i + 1) % edgeListSize);

		auto newVertex = CreateMultiSplitVertex(chainBegin->NextEdge(), chainEnd->PreviousEdge(), center, event->Distance);

		auto beginNextVertex = chainBegin->NextVertex();
		auto endPreviousVertex = chainEnd->PreviousVertex();

#if true // LandscapeCombinator code 
		if (!beginNextVertex || !endPreviousVertex)
			throw std::runtime_error("InvalidOperationException (LC)");
#endif

		CorrectBisectorDirection(newVertex->Bisector, beginNextVertex, endPreviousVertex, chainBegin->NextEdge(), chainEnd->PreviousEdge());

		if (LavUtil::IsSameLav(*beginNextVertex, *endPreviousVertex))
		{
			// if vertex are in same lav we need to cut part of lav in the
			//  middle of vertex and create new lav from that points
			auto lavPart = LavUtil::CutLavPart(beginNextVertex, endPreviousVertex);

			auto lav = std::make_shared<CircularList>();
			sLav->insert(lav);
			lav->AddLast(newVertex);
			for (auto vertex : *lavPart)
				lav->AddLast(vertex);
		}
		else
		{
			//if vertex are in different lavs we need to merge them into one.
			LavUtil::MergeBeforeBaseVertex(beginNextVertex, endPreviousVertex);
			endPreviousVertex->AddNext(endPreviousVertex, newVertex);
		}

		ComputeEvents(newVertex, queue, edges);
		lastFaceNode = AddSplitFaces(lastFaceNode, chainBegin, chainEnd, newVertex);
	}

	// remove all centers of@events from lav
	edgeListSize = chains->size();
	for (size_t i = 0; i < edgeListSize; i++)
	{
		auto chainBegin = chains->at(i);
		auto chainEnd = chains->at((i + 1) % edgeListSize);

		LavUtil::RemoveFromLav(chainBegin->CurrentVertex());
		LavUtil::RemoveFromLav(chainEnd->CurrentVertex());

		if (chainBegin->CurrentVertex() != nullptr)
			chainBegin->CurrentVertex()->IsProcessed = true;
		if (chainEnd->CurrentVertex() != nullptr)
			chainEnd->CurrentVertex()->IsProcessed = true;
	}
}


void SkeletonBuilder::CorrectBisectorDirection(sp<LineParametric2d> bisector, sp<Vertex> beginNextVertex, sp<Vertex> endPreviousVertex, sp<Edge> beginEdge, sp<Edge> endEdge)
{
#if true // LandscapeCombinator code 
	if (!beginNextVertex || !endPreviousVertex)
		throw std::runtime_error("InvalidOperationException (LC)");
#endif

	// New bisector for vertex is created using connected edges. For
	// parallel edges numerical error may appear and direction of created
	// bisector is wrong. It for parallel edges direction of edge need to be
	// corrected using location of vertex.
	auto beginEdge2 = beginNextVertex->PreviousEdge;
	auto endEdge2 = endPreviousVertex->NextEdge;
	if (beginEdge != beginEdge2 || endEdge != endEdge2)
		throw std::runtime_error("InvalidOperationException");

	// Check if edges are parallel and in opposite direction to each other.
	if (beginEdge->Norm->Dot(*endEdge->Norm) < -0.97)
	{
		auto n1 = PrimitiveUtils::FromTo(*endPreviousVertex->Point, *bisector->A).Normalized();
		auto n2 = PrimitiveUtils::FromTo(*bisector->A, *beginNextVertex->Point).Normalized();
		auto bisectorPrediction = CalcVectorBisector(n1, n2);

		// Bisector is calculated in opposite direction to edges and center.
		if (bisector->U->Dot(bisectorPrediction) < 0)
			bisector->U->Negate();
	}
}


std::shared_ptr<FaceNode> SkeletonBuilder::AddSplitFaces(sp<FaceNode> lastFaceNode, sp<IChain> chainBegin, sp<IChain> chainEnd, sp<Vertex> newVertex)
{
	if (typeid(*chainBegin) == typeid(SingleEdgeChain))
	{
		// When chain is generated by opposite edge we need to share face
		// between two chains. Number of that chains shares is always odd.

		// right face
		if (lastFaceNode == nullptr)
		{
			// Vertex generated by opposite edge share three faces, but
			// vertex can store only left and right face. So we need to
			// create vertex clone to store additional back face.
			auto beginVertex = CreateOppositeEdgeVertex(newVertex);

			// same face in two vertex, original and in opposite edge clone
			newVertex->RightFace = beginVertex->RightFace;
			lastFaceNode = beginVertex->LeftFace;
		}
		else
		{
			// face queue exist simply assign it to new node
			if (newVertex->RightFace != nullptr)
				throw std::runtime_error("newVertex.RightFace should be null");

			newVertex->RightFace = lastFaceNode;
			lastFaceNode = nullptr;
		}
	}
	else
	{
		auto beginVertex = chainBegin->CurrentVertex();
		// right face
		AddFaceRight(newVertex, beginVertex);
	}

	if (typeid(*chainEnd) == typeid(SingleEdgeChain))
	{
		// left face
		if (lastFaceNode == nullptr)
		{
			// Vertex generated by opposite edge share three faces, but
			// vertex can store only left and right face. So we need to
			// create vertex clone to store additional back face.
			auto endVertex = CreateOppositeEdgeVertex(newVertex);

			// same face in two vertex, original and in opposite edge clone
			newVertex->LeftFace = endVertex->LeftFace;
			lastFaceNode = endVertex->LeftFace;
		}
		else
		{
			// face queue exist simply assign it to new node
			if (newVertex->LeftFace != nullptr)
				throw std::runtime_error("newVertex.LeftFace should be null.");
			newVertex->LeftFace = lastFaceNode;

			lastFaceNode = nullptr;
		}
	}
	else
	{
		auto endVertex = chainEnd->CurrentVertex();
		// left face
		AddFaceLeft(newVertex, endVertex);
	}
	return lastFaceNode;
}

std::shared_ptr<Vertex> SkeletonBuilder::CreateOppositeEdgeVertex(sp<Vertex> newVertex)
{
	// When opposite edge is processed we need to create copy of vertex to
	// use in opposite face. When opposite edge chain occur vertex is shared
	// by additional output face.
	auto vertex = std::make_shared<Vertex>(newVertex->Point, newVertex->Distance, newVertex->Bisector, newVertex->PreviousEdge, newVertex->NextEdge);

	// create new empty node queue
	auto fn = std::make_shared<FaceNode>(vertex);
	vertex->LeftFace = fn;
	vertex->RightFace = fn;

	// add one node for queue to present opposite site of edge split@event
	auto rightFace = FaceQueue::Create();
	rightFace->AddFirst(fn);
	return vertex;
}

void SkeletonBuilder::CreateOppositeEdgeChains(sp<hashsetCircularList> sLav, sp<listIChain> chains, sp<Vector2d> center)
{
	// Add chain created from opposite edge, this chain have to be
	// calculated during processing@event because lav could change during
	// processing another@events on the same level
	auto oppositeEdges = std::make_shared<std::unordered_set<std::shared_ptr<Edge>, Edge::HashFunction>>(); 

	auto oppositeEdgeChains = std::make_shared<std::vector<std::shared_ptr<IChain>>>(); 
	auto chainsForRemoval = std::make_shared<std::vector<std::shared_ptr<IChain>>>(); 

	for (auto chain : *chains)
	{
		// add opposite edges as chain parts
		if (typeid(*chain) == typeid(SplitChain)) 
		{
			auto splitChain = std::dynamic_pointer_cast<SplitChain>(chain);
			auto oppositeEdge = splitChain->OppositeEdge();
			if (oppositeEdge != nullptr && !(oppositeEdges->find(oppositeEdge) != oppositeEdges->end()) )
			{
				// find lav vertex for opposite edge
				auto nextVertex = FindOppositeEdgeLav(sLav, oppositeEdge, center);
				if (nextVertex != nullptr)
					oppositeEdgeChains->push_back(std::make_shared<SingleEdgeChain>(oppositeEdge, nextVertex));
				else
				{
					FindOppositeEdgeLav(sLav, oppositeEdge, center);
					chainsForRemoval->push_back(chain);
				}
				oppositeEdges->insert(oppositeEdge);
			}
		}
	}

	// if opposite edge can't be found in active lavs then split chain with
	// that edge should be removed
	for (auto chain : *chainsForRemoval)
	{
		for (size_t i = 0; i < chains->size(); i++)
		{
			if (chain == chains->at(i))
			{
				chains->erase(chains->begin() + i);
				i--;
			}
		}
	}

	chains->insert(chains->end(), oppositeEdgeChains->begin(), oppositeEdgeChains->end());
}

std::shared_ptr<Vertex> SkeletonBuilder::CreateMultiSplitVertex(sp<Edge> nextEdge, sp<Edge> previousEdge, sp<Vector2d> center, double distance)
{
	auto bisector = CalcBisector(center, *previousEdge, *nextEdge);
	// edges are mirrored for@event
	return std::make_shared<Vertex>(center, distance, bisector, previousEdge, nextEdge);
}

std::shared_ptr<std::vector<std::shared_ptr<IChain>>> SkeletonBuilder::CreateChains(sp<listSkeletonEvent> cluster)
{
	auto edgeCluster = std::make_shared<std::vector<std::shared_ptr<EdgeEvent>>>(); 
	auto splitCluster = std::make_shared<std::vector<std::shared_ptr<SplitEvent>>>(); 
	auto vertexEventsParents = std::make_shared<std::unordered_set<std::shared_ptr<Vertex>, Vertex::HashFunction>>(); 

	for (auto skeletonEvent : *cluster)
	{
		if (typeid(*skeletonEvent) == typeid(EdgeEvent)) 
		{
			auto e = std::dynamic_pointer_cast<EdgeEvent>(skeletonEvent);
			edgeCluster->push_back(e);
		}
		else
		{
			if (typeid(*skeletonEvent) == typeid(VertexSplitEvent))
			{
				// It will be processed in next loop to find unique split
				// events for one parent.
			}
			else if (typeid(*skeletonEvent) == typeid(SplitEvent))
			{
				auto splitEvent = std::dynamic_pointer_cast<SplitEvent>(skeletonEvent);
				// If vertex and split event exist for the same parent
				// vertex and at the same level always prefer split.
				vertexEventsParents->insert(splitEvent->Parent);
				splitCluster->push_back(splitEvent);
			}
		}
	}

	for (auto skeletonEvent : *cluster)
	{
		if (typeid(*skeletonEvent) == typeid(VertexSplitEvent))
		{
			auto vertexEvent = std::dynamic_pointer_cast<VertexSplitEvent>(skeletonEvent);
			if (!( vertexEventsParents->find(vertexEvent->Parent) != vertexEventsParents->end() ))
			{
				// It can be created multiple vertex events for one parent.
				// Its is caused because two edges share one vertex and new
				//event will be added to both of them. When processing we
				// need always group them into one per vertex. Always prefer
				// split events over vertex events.
				vertexEventsParents->insert(vertexEvent->Parent);
				splitCluster->push_back(vertexEvent);
			}
		}
	}

	auto edgeChains = std::make_shared<std::vector<std::shared_ptr<EdgeChain>>>();

	// We need to find all connected edge events, and create chains from
	// them. Two event are assumed as connected if next parent of one
	// event is equal to previous parent of second event.
	while (edgeCluster->size() > 0)
		edgeChains->push_back(std::make_shared<EdgeChain>(CreateEdgeChain(edgeCluster)));

	auto chains = std::make_shared<std::vector<std::shared_ptr<IChain>>>();  
	for (auto edgeChain : *edgeChains)
		chains->push_back(edgeChain);

splitEventLoop:
	while (!splitCluster->empty())
	{
		auto split = splitCluster->at(0);
		splitCluster->erase(splitCluster->begin() + 0);

		for (auto chain : *edgeChains)
		{
			// check if chain is split type
			if (IsInEdgeChain(split, chain))
				goto splitEventLoop;
		}


		// split event is not part of any edge chain, it should be added as
		// new single element chain;
		chains->push_back(std::make_shared<SplitChain>(split));
	}

	// Return list of chains with type. Possible types are edge chain,
	// closed edge chain, split chain. Closed edge chain will produce pick
	//event. Always it can exist only one closed edge chain for point
	// cluster.
	return chains;
}

bool SkeletonBuilder::IsInEdgeChain(sp<SplitEvent> split, sp<EdgeChain> chain)
{
	auto splitParent = split->Parent;
	auto edgeList = chain->EdgeList;
	return std::any_of(edgeList->begin(), edgeList->end(),
		[splitParent]
	(const std::shared_ptr<EdgeEvent> edgeEvent)
		{
			return edgeEvent->PreviousVertex == splitParent || edgeEvent->NextVertex == splitParent;
		});
}

std::shared_ptr<std::vector<std::shared_ptr<EdgeEvent>>> SkeletonBuilder::CreateEdgeChain(sp<listEdgeEvent> edgeCluster)
{
	auto edgeList = std::make_shared<std::vector<std::shared_ptr<EdgeEvent>>>();

	edgeList->push_back(edgeCluster->at(0));
	edgeCluster->erase(edgeCluster->begin() + 0);

	// find all successors of edge event
	// find all predecessors of edge event
loop:
	for (;;)
	{
		auto beginVertex = edgeList->at(0)->PreviousVertex;
		auto endVertex = edgeList->at(edgeList->size() - 1)->NextVertex;

		for (size_t i = 0; i < edgeCluster->size(); i++)
		{
			auto edge = edgeCluster->at(i);
			if (edge->PreviousVertex == endVertex)
			{
				// edge should be added as last in chain
				edgeCluster->erase(edgeCluster->begin() + i);
				edgeList->push_back(edge);
				goto loop;
			}
			if (edge->NextVertex == beginVertex)
			{
				// edge should be added as first in chain
				edgeCluster->erase(edgeCluster->begin() + i);
				edgeList->insert(edgeList->begin(), edge);
				goto loop;
			}
		}
		break;
	}

	return edgeList;
}

void SkeletonBuilder::RemoveEventsUnderHeight(sp<queueSkeletonEvent> queue, double levelHeight)
{
	while (!queue->empty())
	{
		if (queue->top()->Distance > levelHeight + SplitEpsilon)
			break;
		queue->pop();
	}
}

std::shared_ptr<std::vector<std::shared_ptr<SkeletonEvent>>> SkeletonBuilder::LoadAndGroupLevelEvents(sp<queueSkeletonEvent> queue)
{
	auto levelEvents = LoadLevelEvents(queue);
	return GroupLevelEvents(levelEvents);
}


std::shared_ptr<std::vector<std::shared_ptr<SkeletonEvent>>> SkeletonBuilder::GroupLevelEvents(sp<listSkeletonEvent> levelEvents)
{
	auto ret = std::make_shared<std::vector<std::shared_ptr<SkeletonEvent>>>();

	auto parentGroup = std::make_shared<std::unordered_set<std::shared_ptr<Vertex>, Vertex::HashFunction>>();

	while (levelEvents->size() > 0)
	{
		parentGroup->clear();

		auto event = levelEvents->at(0);
		levelEvents->erase(levelEvents->begin() + 0);
		auto eventCenter = event->V;
		auto distance = event->Distance;

		AddEventToGroup(parentGroup, event);

		auto cluster = std::make_shared<std::vector<std::shared_ptr<SkeletonEvent>>>();
		cluster->push_back(event);

		for (size_t j = 0; j < levelEvents->size(); j++)
		{
			auto test = levelEvents->at(j);

			if (IsEventInGroup(parentGroup, test))
			{
				// Because of numerical errors split event and edge event
				// can appear in slight different point. Epsilon can be
				// apply to level but event point can move rapidly even for
				// little changes in level. If two events for the same level
				// share the same parent, they should be merge together.

				auto item = levelEvents->at(j);
				levelEvents->erase(levelEvents->begin() + j);
				cluster->push_back(item);
				AddEventToGroup(parentGroup, test);
				j--;
			}
			// is near
			else if (eventCenter->DistanceTo(*test->V) < SplitEpsilon)
			{
				// group all event when the result point are near each other
				auto item = levelEvents->at(j);
				levelEvents->erase(levelEvents->begin() + j);
				cluster->push_back(item);
				AddEventToGroup(parentGroup, test);
				j--;
			}
		}

		// More then one event share the same result point, we need to
		// create new level event.
		ret->push_back(CreateLevelEvent(eventCenter, distance, cluster));
	}
	return ret;
}

bool SkeletonBuilder::IsEventInGroup(sp<hashsetVertex> parentGroup, sp<SkeletonEvent> event)
{
	if (typeid(*event) == typeid(SplitEvent))
	{
		auto splitEvent = std::dynamic_pointer_cast<SplitEvent>(event);
		return parentGroup->find(splitEvent->Parent) != parentGroup->end();
	}
	else if (typeid(*event) == typeid(EdgeEvent))
	{
		auto edgeEvent = std::dynamic_pointer_cast<EdgeEvent>(event);
		return ( parentGroup->find(edgeEvent->PreviousVertex) != parentGroup->end() ) || (parentGroup->find(edgeEvent->NextVertex) != parentGroup->end());
	}
	return false;
}

void SkeletonBuilder::AddEventToGroup(sp<hashsetVertex> parentGroup, sp<SkeletonEvent> event)
{

	if (typeid(*event) == typeid(SplitEvent)) 
	{
		auto splitEvent = std::dynamic_pointer_cast<SplitEvent>(event);
		parentGroup->insert(splitEvent->Parent);
	}
	else if (typeid(*event) == typeid(EdgeEvent))
	{
		auto edgeEvent = std::dynamic_pointer_cast<EdgeEvent>(event);
		parentGroup->insert(edgeEvent->PreviousVertex);
		parentGroup->insert(edgeEvent->NextVertex);
	}
}

std::shared_ptr<SkeletonEvent> SkeletonBuilder::CreateLevelEvent(sp<Vector2d> eventCenter, double distance, sp<listSkeletonEvent> eventCluster)
{
	auto chains = CreateChains(eventCluster);

	if (chains->size() == 1)
	{
		auto chain = chains->at(0);
		if (chain->ChainType() == EChainType::INVALID)
			throw std::runtime_error("Something went wrong. Invalid chain type.");
		if (chain->ChainType() == EChainType::CLOSED_EDGE)
			return std::make_shared<PickEvent>(eventCenter, distance, std::dynamic_pointer_cast<EdgeChain>(chain));
		if (chain->ChainType() == EChainType::EDGE)
			return std::make_shared<MultiEdgeEvent>(eventCenter, distance, std::dynamic_pointer_cast<EdgeChain>(chain));
		if (chain->ChainType() == EChainType::SPLIT)
			return std::make_shared<MultiSplitEvent>(eventCenter, distance, chains);
	}

	if (std::any_of(chains->begin(), chains->end(), [](const std::shared_ptr<IChain> chain) {return chain->ChainType() == EChainType::CLOSED_EDGE; }))
		throw std::runtime_error("Found closed chain of events for single point, but found more then one chain");
	return std::make_shared<MultiSplitEvent>(eventCenter, distance, chains);
}

std::shared_ptr<std::vector<std::shared_ptr<SkeletonEvent>>> SkeletonBuilder::LoadLevelEvents(sp<queueSkeletonEvent> queue)
{
	auto level = std::make_shared<std::vector<std::shared_ptr<SkeletonEvent>>>();
	std::shared_ptr<SkeletonEvent> levelStart = nullptr;
	// skip all obsolete events in level
	do
	{
		if (queue->empty())
		{
			levelStart = nullptr;
		}
		else
		{
			levelStart = queue->top();
			queue->pop();
		}
	} while (levelStart != nullptr && levelStart->IsObsolete());


	// all events obsolete
	if (levelStart == nullptr || levelStart->IsObsolete())
		return level;

	auto levelStartHeight = levelStart->Distance;

	level->push_back(levelStart);

	std::shared_ptr<SkeletonEvent> event = nullptr;
	while (!queue->empty() && (event = queue->top()) != nullptr &&
		fabs(event->Distance - levelStartHeight) < SplitEpsilon)
	{
		auto nextLevelEvent = queue->top();
		queue->pop();
		if (!nextLevelEvent->IsObsolete())
			level->push_back(nextLevelEvent);
	}
	return level;
}

int SkeletonBuilder::AssertMaxNumberOfInteraction(int& count)
{
	count++;
	if (count > 10000)
		throw std::runtime_error("Too many interaction: bug?");
	return count;
}

std::vector<std::vector<Vector2d>> SkeletonBuilder::MakeClockwise(nestedlistVector2d& holes)
{
	if (holes.empty())
		return holes;
	for (auto& hole : holes)
	{
		if (!PrimitiveUtils::IsClockwisePolygon(hole))
		{
			std::reverse(hole.begin(), hole.end());
		}
	}
	return holes;
}

std::vector<Vector2d> SkeletonBuilder::MakeCounterClockwise(std::vector<Vector2d>& polygon)
{
	return std::vector<Vector2d>(PrimitiveUtils::MakeCounterClockwise(polygon));
}

void SkeletonBuilder::InitSlav(listVector2d& polygon, sp<hashsetCircularList> sLav, sp<listEdge> edges, sp<listFaceQueue> faces)
{
	CircularList edgesList;
	size_t size = polygon.size();
	for (size_t i = 0; i < size; i++)
	{
		size_t j = (i + 1) % size;
		auto newEdge = std::make_shared<Edge>(polygon.at(i), polygon.at(j));
		edgesList.AddLast(newEdge);
	}
	for (auto edge : edgesList)
	{
		auto nextEdge = std::static_pointer_cast<Edge>(edge->Next);
		auto curEdge = std::static_pointer_cast<Edge>(edge);
		auto end = std::static_pointer_cast<Edge>(edge)->End;
		auto bisector = CalcBisector(end, *curEdge, *nextEdge);
		
		curEdge->BisectorNext = bisector;
		nextEdge->BisectorPrevious = bisector;
		edges->push_back(curEdge);
	}
	std::shared_ptr<CircularList> lav = std::make_shared<CircularList>();
	sLav->insert(lav);
	for (auto edge : edgesList)
	{
		auto curEdge = std::static_pointer_cast<Edge>(edge);
		auto nextEdge = std::static_pointer_cast<Edge>(edge->Next);
		auto vertex = std::make_shared<Vertex>(curEdge->End, 0, curEdge->BisectorNext, curEdge, nextEdge);
		lav->AddLast(vertex);
	}
	for (auto vertex : *lav)
	{
		auto curVertex = std::static_pointer_cast<Vertex>(vertex);
		auto next = std::static_pointer_cast<Vertex>(vertex->Next);
		
		// create face on right site of vertex

		auto rightFace = std::make_shared<FaceNode>(curVertex);

		auto faceQueue = FaceQueue::Create();
		faceQueue->SetEdge(curVertex->NextEdge);

		faceQueue->AddFirst(rightFace);
		faces->push_back(faceQueue);
		curVertex->RightFace = rightFace;

		// create face on left site of next vertex
		auto leftFace = std::make_shared<FaceNode>(next);
		rightFace->AddPush(rightFace, leftFace);
		next->LeftFace = leftFace;
	}
}

Skeleton SkeletonBuilder::AddFacesToOutput(sp<listFaceQueue> faces)
{
	auto edgeOutputs = std::make_shared<std::vector<std::shared_ptr<EdgeResult>>>();
	auto distances = std::make_shared<std::map<Vector2d, double>>();
	for (auto face : *faces)
	{
		if (face->Size() > 0)
		{
			auto faceList = std::make_shared<std::vector<std::shared_ptr<Vector2d>>>();
			for (auto fn : *face)
			{
				auto point = fn->GetVertex()->Point;
				faceList->push_back(point);
				if (!(distances->find(*point) != distances->end())) {
					distances->insert(std::pair<Vector2d, double>(*point, fn->GetVertex()->Distance));
				}

			}
			edgeOutputs->push_back(std::make_shared<EdgeResult>(face->GetEdge(), faceList));
		}
	}
	return Skeleton(edgeOutputs, distances);
}

void SkeletonBuilder::InitEvents(sp<hashsetCircularList> sLav, sp<queueSkeletonEvent> queue, sp<listEdge> edges)
{
	for (auto lav : *sLav)
	{
		for (auto vertex : *lav)
		{
			auto curVertex = std::static_pointer_cast<Vertex>(vertex);
			ComputeSplitEvents(curVertex, edges, queue, -1);
		}
	}
	for (auto lav : *sLav)
	{
		for (auto vertex : *lav)
		{
			auto curVertex = std::static_pointer_cast<Vertex>(vertex);
			auto nextVertex = std::static_pointer_cast<Vertex>(vertex->Next);
			ComputeEdgeEvents(curVertex, nextVertex, queue);
		}
	}
}

void SkeletonBuilder::ComputeSplitEvents(sp<Vertex> vertex, sp<listEdge> edges, sp<queueSkeletonEvent> queue, double distanceSquared)
{
	auto source = vertex->Point;
	auto oppositeEdges = CalcOppositeEdges(vertex, edges);

	// check if it is vertex split event
	for (auto oppositeEdge : *oppositeEdges)
	{
		auto point = oppositeEdge.Point;
		if (fabs(distanceSquared - (-1)) > SplitEpsilon)
		{
			if (source->DistanceSquared(*point) > distanceSquared + SplitEpsilon)
			{
				// Current split event distance from source of event is
				// greater then for edge event. Split event can be reject.
				// Distance from source is not the same as distance for
				// edge. Two events can have the same distance to edge but
				// they will be in different distance form its source.
				// Unnecessary events should be reject otherwise they cause
				// problems for degenerate cases.
				continue;
			}
		}

		// check if it is vertex split event
		if (!oppositeEdge.OppositePoint->IsEmpty())
		{
			// some of vertex event can share the same opposite point
			queue->push(std::make_shared<VertexSplitEvent>(point, oppositeEdge.Distance, vertex));
			continue;
		}
		queue->push(std::make_shared<SplitEvent>(point, oppositeEdge.Distance, vertex, oppositeEdge.OppositeEdge));
	}
}

void SkeletonBuilder::ComputeEvents(sp<Vertex> vertex, sp<queueSkeletonEvent> queue, sp<listEdge> edges)
{
	auto distanceSquared = ComputeCloserEdgeEvent(vertex, queue);
	ComputeSplitEvents(vertex, edges, queue, distanceSquared);
}

double SkeletonBuilder::ComputeCloserEdgeEvent(sp<Vertex> vertex, sp<queueSkeletonEvent> queue)
{
	auto nextVertex = std::static_pointer_cast<Vertex>(vertex->Next);
	auto previousVertex = std::static_pointer_cast<Vertex>(vertex->Previous);

	auto point = vertex->Point;

	// We need to chose closer edge event. When two evens appear in epsilon
	// we take both. They will create single MultiEdgeEvent.
	auto point1 = std::make_shared<Vector2d>(ComputeIntersectionBisectors(vertex, nextVertex));
	auto point2 = std::make_shared<Vector2d>(ComputeIntersectionBisectors(previousVertex, vertex));

	if (point1->IsEmpty() && point2->IsEmpty())
		return -1;

	auto distance1 = DBL_MAX;
	auto distance2 = DBL_MAX;

	if (!point1->IsEmpty())
		distance1 = point->DistanceSquared(*point1);
	if (!point2->IsEmpty())
		distance2 = point->DistanceSquared(*point2);

	if (fabs(distance1 - SplitEpsilon) < distance2)
		queue->push(CreateEdgeEvent(point1, vertex, nextVertex));
	if (fabs(distance2 - SplitEpsilon) < distance1)
		queue->push(CreateEdgeEvent(point2, previousVertex, vertex));

	return distance1 < distance2 ? distance1 : distance2;
}

std::shared_ptr<SkeletonEvent> SkeletonBuilder::CreateEdgeEvent(sp<Vector2d> point, sp<Vertex> previousVertex, sp<Vertex> nextVertex)
{
	return std::make_shared<EdgeEvent>(point, CalcDistance(*point, *previousVertex->NextEdge), previousVertex, nextVertex);
}

void SkeletonBuilder::ComputeEdgeEvents(sp<Vertex> previousVertex, sp<Vertex> nextVertex, sp<queueSkeletonEvent> queue)
{
	auto point = std::make_shared<Vector2d>(ComputeIntersectionBisectors(previousVertex, nextVertex));
	if (!point->IsEmpty())
		queue->push(CreateEdgeEvent(point, previousVertex, nextVertex));
}

std::shared_ptr<Edge> SkeletonBuilder::VertexOpositeEdge(sp<Vector2d> point, sp<Edge> edge)
{
	if (PrimitiveUtils::IsPointOnRay(*point, *edge->BisectorNext, SplitEpsilon))
		return edge;

	if (PrimitiveUtils::IsPointOnRay(*point, *edge->BisectorPrevious, SplitEpsilon))
		return std::static_pointer_cast<Edge>(edge->Previous);
	return nullptr;
}

std::shared_ptr<std::vector<SkeletonBuilder::SplitCandidate>> SkeletonBuilder::CalcOppositeEdges(sp<Vertex> vertex, sp<listEdge> edges)
{
	auto ret = std::make_shared<std::vector<SkeletonBuilder::SplitCandidate>>();
	for (auto edgeEntry : *edges)
	{
		auto edge = edgeEntry->lineLinear2d;
		// check if edge is behind bisector
		if (EdgeBehindBisector(*vertex->Bisector, *edge))
			continue;

		// compute the coordinates of the candidate point Bi
		auto candidatePoint = CalcCandidatePointForSplit(vertex, edgeEntry);
		if (candidatePoint != nullptr)
		{
			ret->push_back(*candidatePoint);
		}
	}
	std::sort(ret->begin(), ret->end(), SkeletonBuilder::SplitCandidateComparer());
	return ret;
}

bool SkeletonBuilder::EdgeBehindBisector(LineParametric2d bisector, LineLinear2d edge)
{
	// Simple intersection test between the bisector starting at V and the
	// whole line containing the currently tested line segment ei rejects
	// the line segments laying "behind" the vertex V
	auto collide = LineParametric2d::Collide(bisector, edge, SplitEpsilon);
	return collide.IsEmpty();
}

std::shared_ptr<SkeletonBuilder::SplitCandidate> SkeletonBuilder::CalcCandidatePointForSplit(sp<Vertex> vertex, sp<Edge> edge)
{
	auto vertexEdge = ChoseLessParallelVertexEdge(vertex, edge);
	if (vertexEdge == nullptr)
		return nullptr;

	const auto& vertexEdteNormNegate = *vertexEdge->Norm;
	auto edgesBisector = CalcVectorBisector(vertexEdteNormNegate, *edge->Norm);
	auto edgesCollide = vertexEdge->lineLinear2d->Collide(*edge->lineLinear2d);

	// Check should be performed to exclude the case when one of the
	// line segments starting at V is parallel to ei.
	if (edgesCollide.IsEmpty())
		throw std::runtime_error("Ups this should not happen");

	auto edgesBisectorLine = std::make_shared<LineLinear2d>(LineParametric2d(edgesCollide, edgesBisector).CreateLinearForm());

	// Compute the coordinates of the candidate point Bi as the intersection
	// between the bisector at V and the axis of the angle between one of
	// the edges starting at V and the tested line segment ei
	auto candidatePoint = std::make_shared<Vector2d>(LineParametric2d::Collide(*vertex->Bisector, *edgesBisectorLine, SplitEpsilon));

	if (candidatePoint->IsEmpty())
		return nullptr;

	if (edge->BisectorPrevious->IsOnRightSite(*candidatePoint, SplitEpsilon)
		&& edge->BisectorNext->IsOnLeftSite(*candidatePoint, SplitEpsilon))
	{
		auto distance = CalcDistance(*candidatePoint, *edge);

		if (edge->BisectorPrevious->IsOnLeftSite(*candidatePoint, SplitEpsilon))
			return std::make_shared<SplitCandidate>(candidatePoint, distance, nullptr, edge->Begin);
		if (edge->BisectorNext->IsOnRightSite(*candidatePoint, SplitEpsilon))
			return std::make_shared<SplitCandidate>(candidatePoint, distance, nullptr, edge->Begin);

		return std::make_shared<SplitCandidate>(candidatePoint, distance, edge, std::make_shared<Vector2d>(Vector2d::Empty()));
	}
	return nullptr;
}

std::shared_ptr<Edge> SkeletonBuilder::ChoseLessParallelVertexEdge(sp<Vertex> vertex, sp<Edge> edge)
{
	auto edgeA = vertex->PreviousEdge;
	auto edgeB = vertex->NextEdge;

	auto vertexEdge = edgeA;

	auto edgeADot = fabs(edge->Norm->Dot(*edgeA->Norm));
	auto edgeBDot = fabs(edge->Norm->Dot(*edgeB->Norm));
	// both lines are parallel to given edge
	if (edgeADot + edgeBDot >= 2 - SplitEpsilon)
		return nullptr;

	// Simple check should be performed to exclude the case when one of
	// the line segments starting at V (vertex) is parallel to e_i
	// (edge) we always chose edge which is less parallel.
	if (edgeADot > edgeBDot)
		vertexEdge = edgeB;

	return vertexEdge;
}

Vector2d SkeletonBuilder::ComputeIntersectionBisectors(sp<Vertex> vertexPrevious, sp<Vertex> vertexNext)
{
	auto bisectorPrevious = vertexPrevious->Bisector;
	auto bisectorNext = vertexNext->Bisector;

	auto intersectRays2d = PrimitiveUtils::IntersectRays2D(*bisectorPrevious, *bisectorNext);
	auto intersect = intersectRays2d.Intersect;

	// skip the same points
	if (*vertexPrevious->Point == intersect || *vertexNext->Point == intersect)
		return Vector2d::Empty();

	return intersect;
}

std::shared_ptr<Vertex> SkeletonBuilder::FindOppositeEdgeLav(sp<hashsetCircularList> sLav, sp<Edge> oppositeEdge, sp<Vector2d>center)
{
	auto edgeLavs = FindEdgeLavs(sLav, oppositeEdge, nullptr);
	return ChooseOppositeEdgeLav(edgeLavs, oppositeEdge, center);
}

std::shared_ptr<Vertex> SkeletonBuilder::ChooseOppositeEdgeLav(sp<listVertex> edgeLavs, sp<Edge> oppositeEdge, sp<Vector2d> center)
{
	if (edgeLavs->empty())
		return nullptr;

	if (edgeLavs->size() == 1)
		return edgeLavs->at(0);

	auto edgeStart = oppositeEdge->Begin;
	auto edgeNorm = oppositeEdge->Norm;
	auto centerVector = *center - *edgeStart;
	auto centerDot = edgeNorm->Dot(centerVector);
	for (auto end : *edgeLavs)
	{
		auto begin = std::static_pointer_cast<Vertex>(end->Previous);

		auto beginVector = *begin->Point - *edgeStart;
		auto endVector = *end->Point - *edgeStart;

		auto beginDot = edgeNorm->Dot(beginVector);
		auto endDot = edgeNorm->Dot(endVector);

		// Make projection of center, begin and end into edge. Begin and end
		// are vertex chosen by opposite edge (then point to opposite edge).
		// Chose lav only when center is between begin and end. Only one lav
		// should meet criteria.
		if ((beginDot < centerDot && centerDot < endDot) ||
			(beginDot > centerDot && centerDot > endDot))
			return end;
	}

	// Additional check if center is inside lav
	for (auto end : *edgeLavs)
	{
		auto size = end->List->Size();
		auto points = std::make_shared<std::vector<std::shared_ptr<Vector2d>>>(); //CHANGED
		auto next = end;
		for (size_t i = 0; i < size; i++)
		{
			points->push_back(next->Point);
			next = std::static_pointer_cast<Vertex>(next->Next);
		}
		if (PrimitiveUtils::IsPointInsidePolygon(*center, points))
			return end;
	}
	throw std::runtime_error("Could not find lav for opposite edge, it could be correct but need some test data to check.");
}

std::shared_ptr<std::vector<std::shared_ptr<Vertex>>> SkeletonBuilder::FindEdgeLavs(sp<hashsetCircularList> sLav, sp<Edge> oppositeEdge, sp<CircularList> skippedLav)
{
	auto edgeLavs = std::make_shared<std::vector<std::shared_ptr<Vertex>>>();
	for (auto lav : *sLav)
	{
		if (lav == skippedLav)
			continue;

		auto vertexInLav = GetEdgeInLav(lav, oppositeEdge);
		if (vertexInLav != nullptr)
			edgeLavs->push_back(vertexInLav);
	}
	return edgeLavs;
}

std::shared_ptr<Vertex> SkeletonBuilder::GetEdgeInLav(sp<CircularList> lav, sp<Edge> oppositeEdge)
{
	for (auto node : *lav)
	{
		auto e = std::static_pointer_cast<Vertex>(node);
		auto epn = std::static_pointer_cast<Edge>(e->Previous->Next);
		if (oppositeEdge == e->PreviousEdge || oppositeEdge == epn)
			return e;
	}
	return nullptr;
}

void SkeletonBuilder::AddFaceBack(sp<Vertex> newVertex, sp<Vertex> va, sp<Vertex> vb)
{
	auto fn = std::make_shared<FaceNode>(newVertex);
	va->RightFace->AddPush(va->RightFace, fn);
	FaceQueueUtil::ConnectQueues(fn, vb->LeftFace);
}

void SkeletonBuilder::AddFaceRight(sp<Vertex> newVertex, sp<Vertex> vb)
{
	std::shared_ptr<FaceNode> fn = std::make_shared<FaceNode>(newVertex);
	vb->RightFace->AddPush(vb->RightFace, fn);
	newVertex->RightFace = fn;
}

void SkeletonBuilder::AddFaceLeft(sp<Vertex> newVertex, sp<Vertex> va)
{
	std::shared_ptr<FaceNode> fn = std::make_shared<FaceNode>(newVertex);
	va->LeftFace->AddPush(va->LeftFace, fn);
	newVertex->LeftFace = fn;
}

double SkeletonBuilder::CalcDistance(Vector2d intersect, Edge currentEdge)
{
	Vector2d edge = *currentEdge.End - *currentEdge.Begin;
	Vector2d vector = intersect - *currentEdge.Begin;
	Vector2d pointOnVector = PrimitiveUtils::OrthogonalProjection(edge, vector);
	return vector.DistanceTo(pointOnVector);
}

std::shared_ptr<LineParametric2d> SkeletonBuilder::CalcBisector(sp<Vector2d> p, Edge e1, Edge e2)
{
	Vector2d norm1 = *e1.Norm;
	Vector2d norm2 = *e2.Norm;
	auto bisector = std::make_shared<Vector2d>(CalcVectorBisector(norm1, norm2));
	return std::make_shared<LineParametric2d>(p, bisector);
}

Vector2d SkeletonBuilder::CalcVectorBisector(Vector2d norm1, Vector2d norm2)
{
	return PrimitiveUtils::BisectorNormalized(norm1, norm2);
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif
