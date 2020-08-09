#include <stdio.h>
#include <stdlib.h>

/*
 * Unless == has been explicitly overloaded, this is the wrong way to compare
 * the value of two strings. What is really being compared here are the values
 * of the two pointers. To compare the values of the strings (which is much 
 * more likely to be what is wanted), use strcmp. The error is classified as 
 * a "model" error, since the user may well have a wrong model about what == 
 * can do. 
 */

void
do_something()
{
	printf("asdf\n");
}

int
main()
{
	

char* string1 = "Hello";
char* string2 = "World";

if (string1 == string2)
  do_something();
	return (0);
}
