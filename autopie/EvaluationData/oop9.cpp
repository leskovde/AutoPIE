#include <iostream>
#include <memory>
#include <cassert>

/*
 * This example contains multiple classes, not all of which
 * are needed to thrown the error.
 */

class FirstClass
{
	float q = 2.71;

public:

	FirstClass()
	{
		std::cout << "First constructor\n";
	}

	~FirstClass()
	{
		std::cout << "First destructor\n";
	}

	float Funct0()
	{
		float result = q;
		return result;
	}
};

class SecondClass
{
	double q = 3.14;
public:
	SecondClass()
	{
		std::cout << "Second constructor\n";
	}

	~SecondClass()
	{
		std::cout << "Second destructor\n";
	}

	double Funct0()
	{
		float result = q * q;
		return result;
	}
};

class Calculation
{
	int timeout_ = 480;

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

class ThirdClass
{
	double* q = nullptr;

public:
	ThirdClass()
	{
		std::cout << "Third constructor\n";
	}

	~ThirdClass()
	{
		std::cout << "Third destructor\n";
	}

	double Funct0()
	{
		float result = (*q) * (*q);
		return result;
	}
};

int main()
{
	auto test1 = FirstClass();
	auto test2 = SecondClass();
	auto test3 = ThirdClass();

	Calculation calc = Calculation(3);

	std::cout << calc.result << "\n";

	std::cout << "First member: " << test1.Funct0() << "\n";
	std::cout << "Second member: " << test2.Funct0() << "\n";
	std::cout << "Third member: " << test3.Funct0() << "\n";

	return 0;
}