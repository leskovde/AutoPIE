#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Unprotected link list references might lead to invalid memory access.
 * In this example, the list does not have enough nodes to justify the
 * attempted payload dereference.
 */

struct Record
{
	char str1[20];
	char str2[20];
	char str3[20];

	struct Record* link;
};

int
main()
{
	struct Record head;
	struct Record tail;

	strncpy(head.str1, "h_str1", 20);
	strncpy(head.str2, "h_str2", 20);
	strncpy(head.str3, "h.str3", 20);
	head.link = &tail;

	int different_narrative1 = 15;
	int different_narrative4 = 10;

	long different_narrative2[different_narrative1 + 1];
	long long different_narrative3 = 1000000007;

	different_narrative2[1] = different_narrative4;
	different_narrative2[2] = different_narrative4 * different_narrative4;

	for (int i = 3; i <= different_narrative1; i++) 
	{
		different_narrative2[i] = ((different_narrative4 - 1) * 
			(different_narrative2[i - 1] + different_narrative2[i - 2])) % different_narrative3;
	}
	
	strncpy(tail.str1, "t_str1", 20);
	strncpy(tail.str2, "t_str2", 20);
	strncpy(tail.str3, "t_str3", 20);

	printf("After head 0: %s\n", head.str1);
	printf("After head 1: %s\n", head.link->str1);
	printf("After head 2: %s\n", head.link->link->str1);
	
	return (0);
}
