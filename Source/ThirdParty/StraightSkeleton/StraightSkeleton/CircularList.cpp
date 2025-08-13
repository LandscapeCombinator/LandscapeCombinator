#include "CircularList.h"

unsigned int CircularList::_idCounter = 0;

CircularList::CircularList()
{
	size = 0;
	_id = ++_idCounter;
	first = nullptr;
}

CircularList::~CircularList()
{
	this->Clear();
}

void CircularList::AddNext(spn node, spn newNode)
{
	if (newNode->List != nullptr)
		throw std::runtime_error("Node is already assigned to different list!");
	

	newNode->List = this;

	newNode->Previous = node;
	newNode->Next = node->Next;

	node->Next->Previous = newNode;
	node->Next = newNode;

	size++;

}

void CircularList::AddPrevious(spn node, spn newNode)
{
	if (newNode->List != nullptr)
		throw std::runtime_error("Node is already assigned to different list!");

	newNode->List = this;

	newNode->Previous = node->Previous;
	newNode->Next = node;

	node->Previous->Next = newNode;
	node->Previous = newNode;

	size++;

}

void CircularList::AddLast(spn node)
{
	if (node->List != nullptr)
		throw std::runtime_error("Node is already assigned to different list!");

	if (first == nullptr)
	{
		first = node;
		node->List = this;
		node->Next = node;
		node->Previous = node;
		size++;
	}
	else
	{
		AddPrevious(first, node);
	}


}

void CircularList::AddLast(Edge edge)
{
	std::shared_ptr<Edge> newNode = std::make_shared<Edge>(Edge(edge));
	AddLast(newNode);
}

void CircularList::AddLast(Vertex vertex)
{
	std::shared_ptr<Vertex> newNode = std::make_shared<Vertex>(Vertex(vertex));
	AddLast(newNode);
}

void CircularList::Remove(spn node)
{
	if (node->List != this)
		throw std::runtime_error("Node is not assigned to this list!");
	if(size <= 0)
		throw std::runtime_error("List is empty can't remove!");


	node->List = nullptr;

	if (size == 1)
	{
		first = nullptr;
	}
	else
	{
		if (first == node)
			first = first->Next;

		node->Previous->Next = node->Next;
		node->Next->Previous = node->Previous;
	}
	node->Previous = nullptr;
	node->Next = nullptr;

	size--;
	
}

void CircularList::Remove(CircularNode* node)
{
	if (node->List != this)
		throw std::runtime_error("Node is not assigned to this list!");
	if (size <= 0)
		throw std::runtime_error("List is empty can't remove!");


	node->List = nullptr;

	if (size == 1)
	{
		first = nullptr;
	}
	else
	{
		if (first.get() == node)
			first = first->Next;

		node->Previous->Next = node->Next;
		node->Next->Previous = node->Previous;
	}
	node->Previous = nullptr;
	node->Next = nullptr;

	size--;
}

void CircularList::Clear()
{
	while (size > 0)
	{
		this->Remove(this->first);
	}
}

size_t CircularList::Size()
{
	return size;
}

std::shared_ptr<CircularNode> CircularList::First()
{
	return first;
}

std::shared_ptr<CircularNode> CircularList::operator[](size_t index)
{
	return GetNode(index);
}

std::shared_ptr<CircularNode> CircularList::GetNode(size_t index) const
{
	if (size != 0 && index <= size - 1)
	{
		spn node = this->first;
		for (size_t i = 0; i < size; i++)
		{
			if (i == index)
			{
				return node;
			}
			else if (node->Next != nullptr)
			{
				node = node->Next;
			}
		}
	}
	return nullptr;
}
bool CircularList::operator==(const CircularList& other) const
{
	return this->_id == other._id;
}
unsigned int CircularList::GetInstanceId() const
{
	return _id;
}

size_t CircularList::HashFunction::operator()(const CircularList& val) const
{
	return val.GetInstanceId();
}

size_t CircularList::HashFunction::operator()(const spl val) const
{
	return val->GetInstanceId();
}
