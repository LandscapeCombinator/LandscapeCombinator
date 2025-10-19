#pragma once
#include <memory>
#include "FaceNode.h"
#include "FaceQueue.h"
#include "Vertex.h"

class FaceQueueUtil
{
private:
	using spfn = std::shared_ptr<FaceNode>;
	static void MoveNodes(spfn firstFace, spfn secondFace);
public:
	static void ConnectQueues(spfn firstFace, spfn secondFace);
};

