#include <stdio.h>
#include <stdlib.h>

/*
 * The result when j=7 and i=6 is (sometimes) -5. Why? You might think there 
 * is a bug in the system routine random. But actually, the error is in the 
 * macro definition. The expansion becomes: 
 * 
 * random()%j-i+1
 *
 * Since % binds more tightly than + or -, we get random()%7 first. If random() 
 * gives a multiple of 7, then random()%7 = 0, 0-6+1 = -5. 
 *
 */

int
main()
{
	

// random returns a random (positive) integer.
// Random returns a random integer in the range 0 to n-1.

#define Random(n)  random()%n

val = Random(j-i+1);
	return (0);
}
