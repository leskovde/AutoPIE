#include <stdio.h>
#include <stdlib.h>

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

	if (foo = 5)
  		foo == 7;
	
	return (0);
}
