#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
 * Oops -- strcpy's first parameter is the destination, and its second 
 * parameter is the source, not the other way around! Whether this is a 
 * dyslexic error or a model error of course depends on what the programmer 
 * believes the proper order to be. 
 */

int
main()
{
	int different_narrative1 = 5;
	int different_narrative2 = 2;
	
	char string1[10] = "Hello";
	char string2[10];

	strcpy(string1, string2);
	
	int different_narrative3 = 0;

	int different_narrative4[different_narrative1 + 1];

	different_narrative4[0] = 1;

	for (int i = 1; i <= different_narrative1; i++)
		different_narrative4[i] = i * different_narrative4[i - 1];

	different_narrative3 = different_narrative4[different_narrative1] / different_narrative4[different_narrative1 - different_narrative2];
	
	assert(string2[0] == 'H');

	printf("%d\n", different_narrative3);
	
	return (0);
}
