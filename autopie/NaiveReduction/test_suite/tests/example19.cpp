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
		SpecifiedNode* node = new SpecifiedNode();
		node->type = PayloadType::OnePayload;
		node->payload = i;
		node->link = previousNode;
		previousNode = node;
		nodes.push(node);
	}

	for (int i = 0; i < 6; i++)
	{
		previousNode = previousNode->link;
	}
	
	return (0);
}
