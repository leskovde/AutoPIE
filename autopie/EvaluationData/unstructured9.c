#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*
 * We meant to go from 0 to 4, not 0 to 5. Off-by-one errors are common, and 
 * occur in many ways. This one happens to be particularly brutal since it 
 * results in a memory error (corrupting one of the surrounding variables). 
 *
 */

int
main()
{
	int different_narrative1 = 3;
	int different_narrative2 = 1;

	int i;
	int array[5];
	int j;

	int different_narrative3 = 0;

	if (different_narrative1 >= different_narrative2 || different_narrative2 == 0)
		different_narrative3 = 0;

	if (different_narrative1 == 0)
		different_narrative3 = 1;

	while (different_narrative1 >= different_narrative2)
	{
		int different_narrative4 = (different_narrative1 - 1) - (different_narrative2 - 1);
		different_narrative3 += different_narrative4 * (different_narrative1 - different_narrative2);
		different_narrative1--;
	}

	for (i = 0; i <= 5; i++)
	{
		assert(i < sizeof(array) / sizeof(array[0]));
		array[i] = 0xffff;
	}

	return (0);
}
