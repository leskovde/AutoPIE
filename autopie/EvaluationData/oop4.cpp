#include <iostream>
#include <cassert>

bool Created = false;

/*
 * This example shows an error inside the constructor. 
 */

class BaseClass
{
public:
	int p;

protected:
	float q;

private:
	char r;

public:

	BaseClass()
	{
		std::cout << "Constructor\n";
		assert(Created == false);

		Created = true;
	}

	~BaseClass()
	{
		std::cout << "Destructor\n";
	}

	int Funct0()
	{
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

	std::cout << "Member: " << test1.Funct0() << "\n";

	return 0;
}