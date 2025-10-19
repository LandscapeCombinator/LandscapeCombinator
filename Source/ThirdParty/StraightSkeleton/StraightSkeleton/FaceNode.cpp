#include "FaceNode.h"
#include "Vertex.h"
#include "FaceQueue.h"

FaceNode::FaceNode(spv vert)
{
	vertex = vert;
}

FaceNode::~FaceNode()
{
	vertex = nullptr;
	Next = nullptr;
	Previous = nullptr;
	List = nullptr;
}

std::shared_ptr<Vertex> FaceNode::GetVertex()
{
	return vertex;
}

bool FaceNode::IsQueueUnconnected()
{
	return List->GetEdge() == nullptr;
}
bool FaceNode::IsEnd()
{
	return Next == nullptr || Previous == nullptr;
}
void FaceNode::AddPush(spfn node, spfn newNode)
{
	List->AddPush(node, newNode);
}
std::shared_ptr<FaceNode> FaceNode::Pop(spfn node)
{
	return List->Pop(node);
}

std::shared_ptr<FaceNode> FaceNode::AddQueue(spfn nodeQueue, spfn queue)
{
	if (List == queue->List)
		return nullptr;

	spfn current(queue);

	while (current != nullptr)
	{
		spfn next(current->Pop(current));
		nodeQueue->AddPush(nodeQueue, current);
		nodeQueue = current;

		current = next;
	}

	return nodeQueue;
}

void FaceNode::QueueClose()
{
	List->Close();
}
