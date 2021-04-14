#include <stdio.h>
#include <stdlib.h>

/*
 * ptr has no storage of its own, its just a pointer. We are writing to space 
 * that ptr happens to be pointing at. If you are lucky, the program will 
 * crash immediately since you are writing to an illegal memory location. If 
 * you are unlucky, the memory location is legal and the program won't crash 
 * until much later! 
 *
 */

int
main()
{
	char* ptr;

	cin >> ptr;
	
	return (0);
}
