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

int different_narrative(int n) 
{ 
    if (n == 1 || n == 2) 
        return 1; 
    else
        return different_narrative(different_narrative(n - 1))  
                + different_narrative(n - different_narrative(n - 1)); 
} 

int
main()
{
	int different_narrative1 = 5;

// random returns a random (positive) integer.
// Random returns a random integer in the range 0 to n-1.

#define Random(n)  random()%n

	int different_narrative2 = different_narrative(different_narrative1);

	val = Random(j-i+1);
	
	assert(val >= 0)
	
	return (0);
}
