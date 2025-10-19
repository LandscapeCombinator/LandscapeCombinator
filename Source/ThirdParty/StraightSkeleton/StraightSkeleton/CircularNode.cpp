#include "CircularNode.h"
#include "CircularList.h"

unsigned int CircularNode::_idCounter = 0;

CircularNode::CircularNode()
{
	_id = ++_idCounter;
	Next = nullptr;
	Previous = nullptr;
	List = nullptr;
}

CircularNode::CircularNode(spn nextNode, spn prevNode, CircularList* list)
{
	_id = ++_idCounter;
	Next = nextNode;
	Previous = prevNode;
	List = list;
}

CircularNode::~CircularNode()
{
	Next = nullptr;
	Previous = nullptr;
}

void CircularNode::AddNext(spn node, spn newNode)
{
	List->AddNext(node, newNode);
}

void CircularNode::AddPrevious(spn node, spn newNode)
{
	List->AddPrevious(node, newNode);
}

void CircularNode::Remove(spn node)
{
	List->Remove(node);
}

std::string CircularNode::ToString() const
{
	return "circular node virtual parent ToString() called";
}

unsigned int CircularNode::GetInstanceId() const
{
	return _id;
}

size_t CircularNode::HashFunction::operator()(const CircularNode& val) const
{
	return val.GetInstanceId();
}

size_t CircularNode::HashFunction::operator()(const std::shared_ptr<CircularNode> val) const
{
	return val->GetInstanceId();
}

bool CircularNode::operator==(const CircularNode& other) const
{
	return this->GetInstanceId() == other.GetInstanceId();
}

bool CircularNode::operator==(const spn other) const
{
	return this->GetInstanceId() == other->GetInstanceId();
}