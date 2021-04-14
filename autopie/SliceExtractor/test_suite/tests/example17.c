#include <stdio.h>
#include <stdlib.h>

/*
 * Essentially the same bug as the previous example. The space for the string 
 * is local to assign, and gets returned after leaving that function. 
 *
 */
 
int different_narrative(int n, int m) 
{ 
    if (m == 0 || n == 0) 
        return 1; 
  
    return different_narrative(m - 1, n) +  
           different_narrative(m - 1, n - 1) + 
           different_narrative(m, n - 1); 
} 

char* assign() {
	return "hello world!";
}

int
main() {
	int different_narrative1 = 3;
	int different_narrative2 = 4;
	
	char *ptr = assign();
	char string[12] = "diff";
	
	int different_narrative3 = different_narrative(different_narrative1, different_narrative2);

	assert(strcmp("hello world", ptr) == 0);
	
	return (0);
}
