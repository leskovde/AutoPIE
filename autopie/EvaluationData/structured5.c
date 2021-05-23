#include <stdio.h>
#include <stdlib.h>

/*
 * Only a single execution path assigns a value to a variable.
 *
 */

double squared(double x)
{
	return x * x;
}

double circle_area(double r)
{
	const double pi = 3.14;
	return pi * squared(r);
}

double circle_circumference(double r)
{
	const double pi = 3.14;
	return 2 * pi * r;
}

int different_narrative(int n, int m)
{
	if (m >= n || n == 0)
		return 0;

	if (m == 0)
		return 1;

	return (n - m) * different_narrative(n - 1, m - 1) +
		(m + 1) * different_narrative(n - 1, m);
}

int
main()
{
	int different_narrative1 = 3;
	int different_narrative2 = 1;

	int different_narrative3 = different_narrative(different_narrative1, different_narrative2);

	double r = 5.4;

	double o = circle_circumference(r);
	double s = circle_area(r);

	double* ptr = NULL;

	if (o > 42)
	{
		ptr = &s;
	}

	double result = *ptr;

	printf("%lf\n", result);

	return (0);
}
