#include <iostream>
#include <memory>
#include <cassert>

/*
 * This example shows an error due to inheritance.
 */

class BaseClass
{
protected:
	float q = 3.14;

public:

	BaseClass()
	{
		std::cout << "Base constructor\n";
	}

	~BaseClass()
	{
		std::cout << "Base destructor\n";
	}

	float Funct0()
	{
		float result = q * q;
		return result;
	}
};

class ChildClass : BaseClass
{
public:
	ChildClass()
	{
		std::cout << "Child constructor\n";
		q = 2.72;
	}

	~ChildClass()
	{
		std::cout << "Child destructor\n";
	}

	float Funct0()
	{
		float result = q * q;
		return result;
	}
};

int main()
{
	auto test1 = std::make_unique<BaseClass>();
	auto test2 = std::make_unique<ChildClass>();

	assert(test1->Funct0() > 9);
	assert(test2->Funct0() > 9);

	std::cout << "Member: " << test1->Funct0() << "\n";

	return 0;
}