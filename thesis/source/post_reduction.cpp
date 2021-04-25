#include <stdio.h>
#include <stdlib.h>

long get_factorial_recursively(int n)
{
	// Missing stopping constraint 
	// => segmentation fault.
	return (n * get_factorial(n - 1));
}

int main()
{
	const int n = 20;
	
	
	
	
	
	
	
	long recursive_result = 
		get_factorial_recursively(n);
	
	
	
	
	
	
	
	
	
	
	
	
}
