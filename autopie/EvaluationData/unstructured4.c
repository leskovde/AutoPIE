#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int
main()
{
	int remaining = 6;
	int computation1 = 0;
	int timeout1 = 360;

	while (timeout1 > 0)
	{
		computation1 = timeout1 & 0xffff;
		computation1 = computation1 << timeout1;

		computation1 |= 1;

		timeout1--;
	}

	remaining--;

	int computation2 = 0;
	int timeout2 = 240;

	while (timeout2 > 0)
	{
		computation2 = timeout2 | 0xffff;
		computation2 = computation2 >> timeout2;

		computation2 &= 1;

		timeout2--;
	}

	remaining--;

	int computation3 = 0;
	int timeout3 = 3600;

	while (timeout3 > 0)
	{
		computation3 = timeout3 + 0xffff;
		computation3 = computation3 & timeout3;

		computation3 = computation3 << 1;
		computation3 = computation3 >> 1;

		timeout3--;
	}

	remaining--;

	int computation4 = 0;
	int timeout4 = 300;

	while (timeout4 > 0)
	{
		computation4 = timeout4 - 0xffff;
		computation4 = computation4 & timeout4;

		computation4 = computation4 & 1;
		computation4 = computation4 | 1;

		timeout4--;
	}

	remaining--;

	int computation5 = 0;
	int timeout5 = 180;

	while (timeout5 > 0)
	{
		computation5 = computation5 & timeout5;
		computation5 -= timeout5 / 0xffff;

		computation5 = computation5 << 1;

		timeout5--;
	}

	remaining--;

	int computation6 = 0;
	int timeout6 = 1800;

	while (timeout6 > 0)
	{
		computation6 += timeout6 / (computation6 + 1);

		computation6 = computation6 & 0xffff;

		timeout6--;
	}

	assert(remaining == 0);

	return (0);
}
