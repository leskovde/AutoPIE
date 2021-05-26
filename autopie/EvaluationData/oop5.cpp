#include <iostream>
#include <cassert>

/*
 * This example shows an error inside a member function.
 */

class BaseClass
{
public:
	int p;

protected:
	float q;

private:
	char r = 'x';

public:

	BaseClass()
	{
		std::cout << "Constructor\n";
	}

	~BaseClass()
	{
		std::cout << "Destructor\n";
	}

	int Funct0()
	{
		assert(r != '\0');
		r = '\0';

		p = 200;
		return p;
	}

private:
	int Funct1()
	{
		std::cout << "Funct1()\n";
		return 0;
	}

protected:
	void Funct2()
	{
		std::cout << "Funct2()\n";
	}
};

int main()
{
	BaseClass test1, test2;

	std::cout << "Member: " << test1.Funct0() << test1.Funct0() << "\n";

	return 0;
}