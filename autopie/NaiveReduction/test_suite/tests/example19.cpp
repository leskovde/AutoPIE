#include <stack>

/**
 * Dead enum values.
 */

enum PayloadType
{
	OnePayload,
	AnotherPayload,
	LastPayload
};

struct Node
{
	int payload;
	Node* link;
};

struct SpecifiedNode : Node
{
	PayloadType type;
};

int
main()
{
	std::stack<Node*> nodes;

	Node* previousNode = nullptr;
	
	for (int i = 0; i < 5; i++)
	{
		Node* node = new SpecifiedNode() { type = PayloadType::OnePayload, payload = i, link = previousNode };
		previousNode = node;
		nodes.push(node);
	}

	for (int i = 0; i < 5; i++)
	{
		previousNode = previousNode->link;
	}
	
	return (0);
}
