#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
 * We didn't explicitly terminate the string, so there is no telling where 
 * the string will end. cout will keep going until it sees a terminator, 
 * regardless of how long it takes. 
 *
 */

int
main()
{
	int different_narrative1 = 15;
	
	int stack_nonzero_up = 0xffff;
	char string[4] = { 'a', 's', 'd', 'f' };
	int stack_nonzero_down = 0xffff;
	
	int different_narrative2 = 0;
	
	int different_narrative3[different_narrative1 + 1];

	different_narrative3[0] = 0;
	different_narrative3[1] = 1;

	for (int i = 2; i <= different_narrative1; i++) {
		if (i % 2 == 0)
			different_narrative3[i] = different_narrative3[i / 2];
		else
			different_narrative3[i] = different_narrative3[(i - 1) / 2] +
			different_narrative3[(i + 1) / 2];
	}

	different_narrative2 = different_narrative3[different_narrative1];

	assert(strcmp(string, "asdf") == 0);

	printf("%d, %s\n", different_narrative2, string);
	
	return (0);
}
