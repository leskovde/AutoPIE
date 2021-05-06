#include <stdio.h>
#include <stdlib.h>

/*
 * Since string is a local variable, its space is lost after returning from 
 * initialize. This space will be reused by the next function to be called. 
 * Eventual disaster! 
 *
 */

char *initialize() {
	char string[80];
	char* ptr = string;
	return ptr;
}

main() {
	char *myval = initialize();
	do_something_with(myval);
}
