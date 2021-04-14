#include <stdio.h>
#include <stdlib.h>

/*
 * Oops! "hello" has a sixth character, the terminator. We just corrupted 
 * one of the surrounding variables on the stack. 
 *
 */

int binomial_coeff(int n, int k) 
{ 
    int C[n + 1][k + 1]; 
  
    for (int i = 0; i <= n; i++) { 
        for (int j = 0; j <= min(i, k); j++) { 
            if (j == 0 || j == i) 
                C[i][j] = 1; 
            else
                C[i][j] = C[i - 1][j - 1] + C[i - 1][j]; 
        } 
    } 
  
    return C[n][k]; 
} 
  
int different_narrative(int n, int m) 
{ 
    return ((2 * m + 1) * binomial_coeff(2 * n, m + n)) / (m + n + 1); 
} 

int
main()
{
	int different_narrative1 = 5;
	int different_narrative2 = 3;
	
	int i = 0xffff;
	char string[5];
	int j = 0xffff;
	
	string = "hello";
	
	int different_narrative3 = different_narrative(different_narrative1, different_narrative2);
	
	assert(j == 0xffff);

	return (0);
}
