#include <iostream>

long get_factorial(int n)
{
	// Missing the stopping 
	// constraint 
	// => segmentation fault.
	return (n * get_factorial(n - 1));
}

int main()
{
	const int n = 20;
	long loop_result = 1;
	
	
	
	
	
	
	
	std::cout << "Success.\n";
	
	return (0);
}
