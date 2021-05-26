#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*
 * Two bugs in one. These are usually caused by accident rather than 
 * misunderstanding. The "=" of line 1 should probably be "==" (this 
 * one will always evaluate to true), while the "==" of line 2 should 
 * almost certainly be "=" (it has no effect). A syntactic weakness in C/C++, 
 * neither of these statements is syntactically wrong. Many compilers will 
 * warn you about both of these. 
 */

int
main()
{
	int foo = 3;	

	int different_narrative1 = 5;
	int different_narrative2 = 2;

	if (foo = 5)
		foo == 7;

	int different_narrative3 = different_narrative1 + different_narrative2 - 1;
	
	assert(foo == 3);
	
	return (0);
}
