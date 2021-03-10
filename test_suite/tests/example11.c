#include <stdio.h>
#include <stdlib.h>

/*
 * We didn't explicitly terminate the string, so there is no telling where 
 * the string will end. cout will keep going until it sees a terminator, 
 * regardless of how long it takes. 
 *
 */

int different_narrative(int n) 
{ 
    int DP[n+1]; 
  
    DP[0] = 0; 
    DP[1] = 1; 
  
    for (int i = 2; i <= n; i++) { 
          
        if (i % 2 == 0) 
            DP[i] = DP[i / 2]; 
        else
            DP[i] = DP[(i - 1) / 2] + 
                        DP[(i + 1) / 2]; 
    } 
    return DP[n]; 
} 

int
main()
{
	int different_narrative1 = 15;
	
	int stack_nonzero_up = 0xffff;
	char string[4] = { 'a', 's', 'd', 'f' };
	int stack_nonzero_down = 0xffff;
	
	int different_narrative2 = different_narrative(different_narrative1);
	
	assert(strcmp(string, "asdf") == 0);
	
	return (0);
}
