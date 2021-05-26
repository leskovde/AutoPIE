#include <iostream>
#include <stack>

/**
 * This example fails due to an null reference error.
 * It also features unused enum values that must be
 * removed during minimization.
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

class Calculation
{
	int timeout_ = 360;

public:
	int result{ 0 };

	explicit Calculation(int x)
	{
		result = Run(x);
	}

	int Run(int x)
	{
		for (int i = 0; i < timeout_; i++)
		{
			if (i % 2)
				x += (int)(0.1 * x * x * i);
			else
				x -= (int)(0.1 * x * x * i);
		}

		return x;
	}
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

	Calculation calc = Calculation(3);

	std::cout << calc.result << "\n";

	for (int i = 0; i < 6; i++)
	{
		previousNode = previousNode->link;
	}
	
	return (0);
}
