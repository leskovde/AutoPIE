#include <stdio.h>
#include <stdlib.h>

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


int
main()
{
	int a = 5;
	int n = 7;

	int b = minval(a, n);

	return (0);
}
