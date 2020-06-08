#include <stdio.h>
#include <stdlib.h>

/*
 * Caused by a stray ";" on line 2. Accidental bugs are often caused by stray
 * characters, etc. While "minor" in their fix, they can be the devil to find! 
 *
 */

int
main()
{
	int i, j;
	int numrows = 5;
	int numcols = 7;
	int pixels;

	for (i=0; i<numrows; i++)
  		for (j=0; j<numcols; j++);
			pixels++;

	return (0);
}
