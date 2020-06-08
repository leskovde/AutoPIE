#include <stdio.h>
#include <stdlib.h>

/*
 * Oops! "hello" has a sixth character, the terminator. We just corrupted 
 * one of the surrounding variables on the stack. 
 *
 */

int
main()
{
	
	int i;
	char string[5] = "hello";
	int j;

	return (0);
}
