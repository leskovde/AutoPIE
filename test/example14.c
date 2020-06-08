#include <stdio.h>
#include <stdlib.h>

/*
 * We meant to go from 0 to 4, not 0 to 5. Off-by-one errors are common, and 
 * occur in many ways. This one happens to be particularly brutal since it 
 * results in a memory error (corrupting one of the surrounding variables). 
 *
 */

int
main()
{

	int i;
	int array[5];
	int j;

	for (i=0; i<=5; i++)
  		cin >> array[i];
	
	return (0);
}
