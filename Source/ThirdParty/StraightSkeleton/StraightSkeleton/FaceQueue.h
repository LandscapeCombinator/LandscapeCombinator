#pragma once
#include "Edge.h"
#include "FaceNode.h"

class FaceQueue : public std::enable_shared_from_this<FaceQueue>
{
private:	
    using spfn = std::shared_ptr<FaceNode>;	
	using spe = std::shared_ptr<Edge>;
	using spfq = std::shared_ptr<FaceQueue>;
	spe edge = nullptr;
	bool closed = false;
	size_t size = 0;
	spfn first = nullptr;
	FaceQueue() = default;
public:
	struct Iterator
	{
		spfn first;
		size_t s;
		size_t i;
		spfn operator*()
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
		Iterator& operator = (spfn rhs)
		{
			this->first = rhs;
			return *this;
		}
	};
	Iterator begin()
	{ 
		if(first->IsEnd())
			return Iterator{ first, size };

		auto current = first;
		while(current->Previous != nullptr)
			current = current->Previous;
			
		return Iterator{ current, size };
	}
	Iterator end() 
	{ 
		return Iterator{ nullptr, size }; 
	}
	Iterator cbegin() 
	{ 
		if (first->IsEnd())
			return Iterator{ first, size };

		auto current = first;
		while (current->Previous != nullptr)
			current = current->Previous;

		return Iterator{ current, size };
	}
	Iterator cend() 
	{ 
		return Iterator{ nullptr, size }; 
	}
	[[nodiscard]] static spfq Create();
	~FaceQueue();
	spfq getptr();
    spe GetEdge();	
    bool Closed();
    bool IsUnconnected();
    void AddPush(spfn node, spfn newNode);
    void AddFirst(spfn node);
	spfn Pop(spfn node);
    void SetEdge(spe val);
    void Close();
	void Clear();
    size_t Size();
	spfn First();
};

