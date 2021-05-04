#include <stdio.h>
#include <stdlib.h>

/*
 * Record city is being used for all of the data fields of all lnodes created 
 * by insert. Of course, there really is only one copy of city, so all of the 
 * lnodes have the same information! insert needs to create a Record (using 
 * new) for each lnode. 
 *
 */

// TODO : ADD WRAPPER (LINKED LIST) CODE

main() {
	Record city;
	lnode *list = NULL;

	while (data_to_read()) {
		Readin_data(&city);
		insert(&city, &list);
  }
}

void insert(Record*& city, lnode*& list) {
  lnode* ptr = new lnode;
  ptr->next = list;
  list = ptr;
  prt->data = city;
}
