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

int different_narrative(int n) 
{ 
	if (n == 1)
		return 1; 
  
	return 1 + different_narrative(n -  
		different_narrative(different_narrative(n - 1))); 
} 

int
main()
{
	int different_narrative1 = 6;

	char* string1 = "Hello World";
	char* string2 = "Hello World";
	
	int different_narrative2 = different_narrative(different_narrative1);

	assert(string1 == string2);
	
	return (0);
}
