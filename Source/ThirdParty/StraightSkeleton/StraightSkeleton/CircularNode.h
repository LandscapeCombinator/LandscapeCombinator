#pragma once
#include <string>
#include <memory>

class CircularList;
class CircularNode
{
protected:
	unsigned int _id;
	static unsigned int _idCounter;
private:
	using spn = std::shared_ptr<CircularNode>;	
public:
	struct HashFunction
	{
		size_t operator()(const CircularNode& val) const;
		size_t operator()(const std::shared_ptr<CircularNode> val) const;
	};
	friend class CircularList;
	CircularList* List;
	spn Next = nullptr;
	spn Previous = nullptr;
	CircularNode();
	CircularNode(spn nextNode, spn prevNode, CircularList* list);
	virtual ~CircularNode();
	void AddNext(spn node, spn newNode);
	void AddPrevious(spn node, spn newNode);
	void Remove(spn node);
	virtual std::string ToString() const;	
	CircularNode& operator=(const CircularNode& val);
	bool operator==(const CircularNode& other) const;
	bool operator==(const spn other) const;
	unsigned int GetInstanceId() const;
};

