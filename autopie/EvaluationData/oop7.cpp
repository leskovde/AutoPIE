#include <iostream>
#include <memory>
#include <cassert>

/*
 * This example shows an error due to overloading.
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

	float Funct0(int x)
	{
		float result = q * q;
		return 0.00025 * x * result;
	}

	float Funct0(float x)
	{
		float result = q * q;
		return 5 * x * result;
	}

	float Funct0(double x)
	{
		float result = q * q;
		return 10 * x * result;
	}
};

int main()
{
	auto test1 = std::make_unique<BaseClass>();

	int x = 5;
	float y = 5;
	double z = 5;

	std::cout << "Member int: " << test1->Funct0(x) << "\n";
	std::cout << "Member float: " << test1->Funct0(y) << "\n";
	std::cout << "Member double: " << test1->Funct0(z) << "\n";

	float result = test1->Funct0(x + 0.5 - 0.5);

	assert(result < x);

	return 0;
}