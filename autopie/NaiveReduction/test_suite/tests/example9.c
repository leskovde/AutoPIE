#include <stdio.h>
#include <stdlib.h>

/*
 * The *curr-> construct should be (*curr)-> (in two places). The reason 
 * is that -> binds more tightly than * in operator precedence, but the 
 * user probably did not realize this. Again, this is classified as a "model" 
 * error since the user probably believes (implicitly) that the operator 
 * precedence goes the other way. Of course, you could avoid the problem 
 * entirely in this particular example by using pass-by-reference in C++. 
 *
 */

// Return pointer to the node storing "val" if any; NULL otherwise
void find(listnode **curr, val) {
	while (*curr != NULL)
		if (*curr->val == val) return;
	else
		*curr = *curr->next;
}

int
main()
{
	// TODO: DRIVER CODE

	return (0);
}
