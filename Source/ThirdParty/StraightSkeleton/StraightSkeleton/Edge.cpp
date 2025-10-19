#include "Edge.h"
#include "CircularList.h"

Edge::Edge()
{
	Begin = nullptr;
	End = nullptr;
	lineLinear2d = nullptr;
	Norm = nullptr;
}

Edge::Edge(Vector2d begin,Vector2d end, spn nextNode, spn prevNode, CircularList* list) : CircularNode(nextNode, prevNode, list)
{
	Begin = std::make_shared<Vector2d>(begin); 					 
	End = std::make_shared<Vector2d>(end); 						 
	lineLinear2d = std::make_shared<LineLinear2d>(begin, end); 
	Norm = std::make_shared<Vector2d>((end - begin).Normalized()); 
}

Edge::Edge(spv2d begin, spv2d end)
{
	Begin = begin;
	End = end;
	lineLinear2d = std::make_shared<LineLinear2d>(*begin, *end);
	Norm = std::make_shared<Vector2d>((*end - *begin).Normalized());
}

std::string Edge::ToString() const
{
	return std::string("Edge " + Begin->ToString() + " " + End->ToString());
}

Edge& Edge::operator=(const Edge& other)
{
	if (this != &other)
	{
		Begin = other.Begin;
		End = other.End;
		lineLinear2d = other.lineLinear2d;
		Norm = other.Norm;
	}
	return *this;
};

Edge::~Edge()
{
	Begin = nullptr;
	End = nullptr;
	Norm = nullptr;
	BisectorNext = nullptr;
	BisectorPrevious = nullptr;
	lineLinear2d = nullptr;
}