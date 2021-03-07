#include <stdio.h>
#include <stdlib.h>

/*
 * Oops -- strcpy's first parameter is the destination, and its second 
 * parameter is the source, not the other way around! Whether this is a 
 * dyslexic error or a model error of course depends on what the programmer 
 * believes the proper order to be. 
 */

int different_narrative(int n, int k) 
{ 
    int fact[n + 1]; 
  
    fact[0] = 1; 
  
    for (int i = 1; i <= n; i++) 
        fact[i] = i * fact[i - 1]; 
  
    return fact[n] / fact[n - k]; 
} 

int
main()
{
	int different_narrative1 = 5;
	int different_narrative2 = 2;
	
	char string1[10] = "Hello";
	char string2[10];

	strcpy(string1, string2);
	
	int different_narrative3 = different_narrative(different_narrative1, different_narrative2);
	
	assert(string2[0] == 'H');
	
	return (0);
}
