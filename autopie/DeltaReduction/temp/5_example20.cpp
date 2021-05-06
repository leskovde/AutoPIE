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
	
	long loop_result = 1;
	
	
	
	
	
	
	
	std::cout << "Success.\n";
	
	
}
