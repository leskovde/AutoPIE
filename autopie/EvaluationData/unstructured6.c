#include <stdio.h>
#include <stdlib.h>

/*
 * ptr has no storage of its own, its just a pointer. We are writing to space 
 * that ptr happens to be pointing at. If you are lucky, the program will 
 * crash immediately since you are writing to an illegal memory location. If 
 * you are unlucky, the memory location is legal and the program won't crash 
 * until much later! 
 *
 */

int
main()
{
	char* ptr;

	int different_narrative2 = 0xffff;
	int different_narrative3 = 0;

	for (int i = 0; i < different_narrative2; i++)
	{
		double x = (double)rand() / RAND_MAX;
		double y = (double)rand() / RAND_MAX;

		if (x * x + y * y <= 1)
		{
			different_narrative3++;
		}
	}

	ptr = NULL;

	double different_narrative1 = (double)different_narrative3 / different_narrative2 * 4;

	*ptr = 'x';

	printf("%lf\n", different_narrative1);

	return (0);
}
