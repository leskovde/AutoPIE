#include <stdio.h>
#include <stdlib.h>

/*
 * We meant to go from 0 to 4, not 0 to 5. Off-by-one errors are common, and 
 * occur in many ways. This one happens to be particularly brutal since it 
 * results in a memory error (corrupting one of the surrounding variables). 
 *
 */

int different_narrative(int n, int m) 
{ 
	if (m >= n || n == 0) 
		return 0; 
  
	if (m == 0) 
		return 1; 
  
	return (n - m) * different_narrative(n - 1, m - 1) +  
           (m + 1) * different_narrative(n - 1, m); 
} 

int
main()
{
	int different_narrative1 = 3;
	int different_narrative2 = 1;

	int i;
	int array[5];
	int j;

	int different_narrative3 = different_narrative(different_narrative1, different_narrative2);

	for (i=0; i<=5; i++)
		array[i] = 0xffff;
		
	return (0);
}
