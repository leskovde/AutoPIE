#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*
 * This example introduces useless function calls and branching.
 */

int initialize1()
{
	return 1;
}

double initialize2()
{
	return 2.0;
}

char initialize3()
{
	return 3;
}

void do_nothing1(int i)
{
	(void)i;
}

void do_nothing2(double i)
{
	(void)i;
}

void do_nothing3(char i)
{
	(void)i;
}

int
main()
{
	int k = 5;
	int result = 0;

	for (int i = 0; i < k; i++)
	{
		if (i % 2 == 0)
		{
			int x = initialize1();
			do_nothing1(x);
			result += (int)x;
		}
		else
		{
			result -= 1;
		}

		if (i % 3 == 0)
		{
			double x = initialize2();
			do_nothing2(x);
			result += (int)x;
			do_nothing2(x);
		}
		else
		{
			result -= 2;
		}

		if (i % 4 == 0)
		{
			char x = initialize3();
			do_nothing3(x);
			result += (int)x;
		}
		else
		{
			if (i % 3)
			{
				result--;
			}
			else
			{
				result -= 3;
			}
		}
	}

	assert(result != 0);

	printf("%d\n", result);

	return (0);
}
