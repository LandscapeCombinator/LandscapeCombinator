#pragma once
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <memory>
#include "Edge.h"
#include "Vertex.h"
#include "CircularNode.h"

class CircularList
{
private:
	using spn = std::shared_ptr<CircularNode>;
	using spl = std::shared_ptr<CircularList>;
	unsigned int _id;
	static unsigned int _idCounter;
	spn first = nullptr;
	size_t size = 0;
	spn GetNode(size_t index) const;
public:	
	struct HashFunction
	{
		size_t operator()(const CircularList& val) const;
		size_t operator()(const spl val) const;
	};
	struct Iterator 
	{
		spn first;
		size_t s;
		size_t i;
		spn operator*()
		{ 
			return first;
		}
		bool operator != (const Iterator& rhs)
		{
			s = rhs.s;			
			return first != rhs.first;
		}
		bool operator == (const Iterator& rhs)
		{
			return first == rhs.first;
		}
		Iterator& operator++() 
		{ 
			i++;
			if (i < s)
				first = first->Next;
			else
				first = nullptr;
			return *this;
		}
		Iterator operator++(int)
		{
			Iterator iterator = *this;
			++* this;
			return iterator;
		}
		Iterator& operator = (spn rhs)
		{
			this->first = rhs;
			return *this;
		}
	};
	Iterator begin() { return Iterator{ first, size }; }
	Iterator end() { return Iterator{ nullptr, size }; }
	Iterator cbegin() { return Iterator{ first, size }; }
	Iterator cend() { return Iterator{ nullptr, size }; }
	
	CircularList();
	~CircularList();
	void AddNext(spn node, spn newNode);
	void AddPrevious(spn node, spn newNode);
	void AddLast(spn node);
	void AddLast(Edge edge);
	void AddLast(Vertex vertex);
	void Remove(spn node);
	void Remove(CircularNode* node);
	void Clear();		
	size_t Size();
	spn First();
	spn operator[](size_t index);
	bool operator==(const CircularList& other) const;
	unsigned int GetInstanceId() const;
};
