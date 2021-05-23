#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*
 * Since string is a local variable, its space is lost after returning from 
 * initialize. This space will be reused by the next function to be called. 
 * Eventual disaster! 
 *
 */

int process(char* str)
{
	int count = 0;
	char* c = str;

	while (*c != '\0')
	{
		count++;
		c++;
	}

	return count;
}

char *initialize() {
	char string[80];
	string[0] = 'x';
	char* ptr = string;
	return ptr;
}

char* initialize2() {
	char string[80] = { 0 };
	char* ptr = string;
	return ptr;
}

int main() {
	char* str = initialize();
	char* str2 = initialize2();

	int x = 5;

	int count = process(str);

	assert(count > 0);

	printf("%s\n%d\n", str, count);

	return (0);
}
