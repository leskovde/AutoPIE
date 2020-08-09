#include <stdio.h>
#include <stdlib.h>

/*
 * We didn't explicitly terminate the string, so there is no telling where 
 * the string will end. cout will keep going until it sees a terminator, 
 * regardless of how long it takes. 
 *
 */

int
main()
{
	char string[4];

	for (i=0; i<4; i++)
  		string[i]=getchar();
	cout << string;
	
	return (0);
}
