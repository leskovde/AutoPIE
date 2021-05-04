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

int different_narrative(int n, int k)
{
	if (k > n)
		return 0;
	if (k == 0 || k == n)
		return 1;
 
	return different_narrative(n - 1, k - 1)
		+ different_narrative(n - 1, k);
}

int
main()
{
	int foo = 3;	

	int different_narrative1 = 5;
	int different_narrative2 = 2;

	if (foo = 5)
		foo == 7;

	int different_narrative3 = different_narrative(different_narrative1, different_narrative2);
	
	assert(foo == 3);
	
	return (0);
}
