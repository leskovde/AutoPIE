#include <stdio.h>
#include <stdlib.h>

/*
 * Here are three errors. First, if ptr happens to be NULL, we can't follow 
 * to its next field. Second, if ptr points to a node, but that node is the 
 * last on its list, then we can't go to ptr->next->next. Third, we just 
 * dropped the space for the deleted node into the bit-bucket: OK in JAVA 
 * or LISP, but not in C or C++! 
 *
 */

// Delete the node following the one that ptr is pointing at.
void del_link(lnode* ptr) {
  ptr->next = ptr->next->next;
}


int
main()
{
	//TODO: ADD DRIVER CODE 	

	return (0);
}
