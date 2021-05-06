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
	
	for (int i = 1; i <= n; 
		i++)
	{
		loop_result *= i;
	}
	
	
	
	
	
	
	
	return (0);
}
