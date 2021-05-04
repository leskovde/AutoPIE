#include <stdio.h>
#include <stdlib.h>

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

	head.str1 = "h_str1";
	head.str2 = "h_str2";
	head.str3 = "h.str3";
	head.link = &tail;
	
	tail.str1 = "t_str1";
	tail.str2 = "t_str2";
	tail.str3 = "t_str3";

	printf("After head 0: %s\n", head.str1);
	printf("After head 1: %s\n", head.link->str1);
	printf("After head 2: %s\n", head.link->link->str1);
	
	return (0);
}
