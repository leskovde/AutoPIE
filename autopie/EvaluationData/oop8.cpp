#include <iostream>
#include <memory>
#include <cassert>

/*
 * This example shows an error due to polymorphism.
 */

class BaseClass
{
protected:
	float q = 2.71;

public:

	BaseClass()
	{
		std::cout << "Base constructor\n";
	}

	~BaseClass()
	{
		std::cout << "Base destructor\n";
	}

	virtual float Funct0()
	{
		float result = q;
		return result;
	}
};

class ChildClass : public BaseClass
{
public:
	ChildClass()
	{
		std::cout << "Child constructor\n";
	}

	~ChildClass()
	{
		std::cout << "Child destructor\n";
	}

	float Funct0() override
	{
		float result = q * q;
		return result;
	}
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

int main()
{
	std::unique_ptr<BaseClass> test1 = std::make_unique<BaseClass>();
	std::unique_ptr<BaseClass> test2 = std::make_unique<ChildClass>();

	Calculation calc = Calculation(3);

	std::cout << calc.result << "\n";

	assert(test1->Funct0() < 3);
	assert(test2->Funct0() < 3);

	std::cout << "Member: " << test1->Funct0() << "\n";

	return 0;
}