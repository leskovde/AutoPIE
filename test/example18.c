#include <stdio.h>
#include <stdlib.h>

/*
 * The goal here is to change the value of the pointer passed in when we do 
 * the insert to the list. The assignment curr = curr->next is in error 
 * because this changes its alias as well (curr is passed by reference). 
 * Instead, the recursive call should read insert(curr->next, val);, 
 * effectively working on a local variable (curr->next) without then 
 * modifying the current recursion variable (curr). 
 *
 */

// Insert a value into an ordered linked list
void insert(lnode*& curr, int val) {
  if (curr == NULL)
    curr = new lnode(val, NULL);
  else if (lnode->val > val)
    curr = new lnode(val, curr->next);
  else {
    curr = curr->next;
    insert(curr, val);
  }
}

int
main()
{

	//TODO: ADD DRIVER CODE 

	return (0);
}
