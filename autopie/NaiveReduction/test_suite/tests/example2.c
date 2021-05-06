#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*
 * Since currmin was never initialized, it could easily start out as the 
 * minimum value. Some compilers spot no-initialization errors. Note that 
 * an improper initialization, while rarer, is even harder to spot than 
 * a missing one! 
 *
 */

int minval(int *A, int n) {
	int currmin;

	for (int i=0; i<n; i++)
		if (A[i] < currmin)
			currmin = A[i];
	return currmin;
}

unsigned long int different_narrative(unsigned int n)
{
	if (n <= 1)
		return 1;
 
	unsigned long int res = 0;
	for (int i = 0; i < n; i++)
		res += different_narrative(i) 
			* different_narrative(n - i - 1);
 
	return res;
}

int
main()
{
	int a[] = {5, 6, 7, 8, 9, 10, 11};
	int n = 7;
	
	unsigned int different_narrative1 = 12;
	unsigned long int different_narrative2 = different_narrative(different_narrative1);

	int b = minval(a, n);
	
	assert(b == 5);

	return (0);
}
