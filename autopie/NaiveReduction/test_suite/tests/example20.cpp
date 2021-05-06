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
	
	long recursive_result = get_factorial(n);
	
	if (loop_result != recursive_result)
	{
		std::cout << loop_result << "\n"; 
		std::cout << recursive_result << "\n";
			
		return (1);
	}
	
	std::cout << "Success.\n";
	
	return (0);
}
