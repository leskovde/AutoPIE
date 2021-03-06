#include <stdio.h>
#include <stdlib.h>

/*
 * The cases were generated by copying case 1. Under case 3, the values were 
 * not changed as appropriate for the case. Code reuse is good -- but this 
 * form of code copying has its dangers!
 *
 */
 
int global_i = 0;

void
do_something(int i)
{
	global_i = i;
}

int
main()
{
	int i = 3;

	switch (i) {
  	case 1:
    		do_something(1); break;
  	case 2:
    		do_something(2); break;
  	case 3:
    		do_something(1); break;
  	case 4:
    		do_something(4); break;
  	default:
    		break;
	}
	
	assert(global_i == i);
	
	return (0);
}
