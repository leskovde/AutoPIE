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
	long loop_result = 1;
	
	for (int i = 1; i <= n; i++)
	{
		loop_result *= i;
	}
	
	long recursive_result = 
		get_factorial_recursively(n);
	
	if (loop_result != recursive_result)
	{
		printf("Diff: %ld from %ld\n", 
			loop_result, recursive_result);
			
		return (1);
	}
	
	printf("Success.\n");
	
	return (0);
}
