#include "FaceQueue.h"

std::shared_ptr<FaceQueue> FaceQueue::getptr()
{
    return shared_from_this();
}

std::shared_ptr<FaceQueue> FaceQueue::Create()
{
    return std::shared_ptr<FaceQueue>(new FaceQueue());
}

FaceQueue::~FaceQueue()
{
    edge = nullptr;
    Clear();
}

std::shared_ptr<Edge> FaceQueue::GetEdge()
{
    return edge;
}

bool FaceQueue::Closed()
{
    return closed;
}

bool FaceQueue::IsUnconnected()
{
    return edge == nullptr;
}

void FaceQueue::AddPush(spfn node, spfn newNode)
{
    if (Closed())
    {
        throw std::runtime_error("Can't add node to closed FaceQueue");
    }
    
    if (newNode->List != nullptr)
    {
        throw std::runtime_error("Node is already assigned to different list!");
    }

    if (node->Previous != nullptr && node->Next != nullptr)
    {
        throw std::runtime_error("Can't push new node. Node is inside a Queue. New node can by added only at the end of queue.");
    }
    
    newNode->List = this->getptr();
    size++;

    if (node->Next == nullptr)
    {
        newNode->Previous = node;
        newNode->Next = nullptr;

        node->Next = newNode;
    }
    else
    {
        newNode->Previous = nullptr;
        newNode->Next = node;

        node->Previous = newNode;
    }
}

void FaceQueue::AddFirst(spfn node)
{
    if (node->List != nullptr)
        throw std::runtime_error("Node is already assigned to different list!");

    if (first == nullptr)
    {
        first = node;

        node->List = this->getptr();
        node->Next = nullptr;
        node->Previous = nullptr;

        size++;
    }
    else
        throw std::runtime_error("First element already exist!");
    
}

std::shared_ptr<FaceNode> FaceQueue::Pop(spfn node)
{
    if (node->List.get() != this)
        throw std::runtime_error("Node is not assigned to this list!");
    if (size <= 0)
        throw std::runtime_error("List is empty can't remove!");
    if(!node->IsEnd())
        throw std::runtime_error("Can pop only from end of queue!");


    node->List = nullptr;

    spfn previous = nullptr;

    if (size == 1)
        first = nullptr;
    else
    {
        if (first == node)
        {
            if (node->Next != nullptr)
                first = node->Next;
            else if (node->Previous != nullptr)
                first = node->Previous;
            else
                throw std::runtime_error("Ups ?");
        }
        if (node->Next != nullptr)
        {
            node->Next->Previous = nullptr;
            previous = node->Next;
        }
        else if (node->Previous != nullptr)
        {
            node->Previous->Next = nullptr;
            previous = node->Previous;
        }
    }

    node->Previous = nullptr;
    node->Next = nullptr;

    size--;
    return previous;
}

void FaceQueue::SetEdge(spe val)
{
    edge = val;
}

void FaceQueue::Close()
{
    closed = true;
}

void FaceQueue::Clear()
{
    while (size > 0)
        Pop(first);
}

size_t FaceQueue::Size()
{
    return size;
}

std::shared_ptr<FaceNode> FaceQueue::First()
{
    return first;
}
