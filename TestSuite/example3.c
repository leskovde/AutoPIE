#include <stdio.h>
#include <stdlib.h>

/*
 * Here, the ">" on line 5 should be "<". Even people who are not normally 
 * dyslexic are subject to these types of errors. 
 *
 */

int minval(int *A, int n) {
  int currmin = MAXINT;

  for (int i=0; i<n; i++)
    if (A[i] > currmin)
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
