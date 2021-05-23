#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*
 * Unless == has been explicitly overloaded, this is the wrong way to compare
 * the value of two strings. What is really being compared here are the values
 * of the two pointers. To compare the values of the strings (which is much 
 * more likely to be what is wanted), use strcmp. The error is classified as 
 * a "model" error, since the user may well have a wrong model about what == 
 * can do. 
 */

int
main()
{
	int different_narrative1 = 6;

	char* string1 = "Hello World";
	
	int different_narrative2 = 0;

	for (int i = 1; i < different_narrative1; i++)
	{
		int inc = 1 + i - 1 + i - 2;
		different_narrative2 += inc;
	}

	int different_narrative3 = 0;

	for (int i = 1; i < different_narrative2; i++)
	{
		different_narrative3 *= (4 * i - 2);
		different_narrative3 /= (i + 1);
	}

	char* string2 = "Hello World!";

	assert(string1 == string2);

	printf("%d\n%s\n", different_narrative3, string1);

	return (0);
}
