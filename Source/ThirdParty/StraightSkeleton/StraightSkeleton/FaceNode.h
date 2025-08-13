#pragma once
#include <memory>

class Vertex;
class FaceQueue;
class FaceNode
{	
private:
	using spv = std::shared_ptr<Vertex>;
	using spfn = std::shared_ptr<FaceNode>;
	using spfq = std::shared_ptr<FaceQueue>;
	spv vertex = nullptr;
public:
	friend class FaceQueue;
	spfn Next = nullptr;
	spfn Previous = nullptr;
	spfq List = nullptr;
	FaceNode(spv vert);
	~FaceNode();
	std::shared_ptr<Vertex> GetVertex();
	bool IsQueueUnconnected();
	bool IsEnd();
	void AddPush(spfn node, spfn newNode);
	spfn Pop(spfn node);
	spfn FindEnd();
	spfn AddQueue(spfn nodeQueue, spfn queue);
	void QueueClose();
};

