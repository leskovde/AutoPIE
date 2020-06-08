#include <stdio.h>
#include <stdlib.h>

/*
 * Oops -- strcpy's first parameter is the destination, and its second 
 * parameter is the source, not the other way around! Whether this is a 
 * dyslexic error or a model error of course depends on what the programmer 
 * believes the proper order to be. 
 */

int
main()
{
	
char string1[10] = "Hello";
char string2[10];

strcpy(string1, string2);
	return (0);
}
