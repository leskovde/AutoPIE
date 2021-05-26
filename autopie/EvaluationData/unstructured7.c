#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int
main()
{
	int different_narrative1 = 5;
	int different_narrative2 = 0;

	while (different_narrative1 > 0)
	{
		int inc = 0;

		if (different_narrative1 == 1 || different_narrative1 == 2)
		{
			inc = 1;
		}
		else
		{
			inc = (different_narrative1 - 1) * different_narrative1;
		}

		different_narrative2 += inc;
		different_narrative1--;
	}

	char different_narrative8[] = "aaaabaaaba";
	char different_narrative9[] = "aaaabaaa";

	int n = 10;
	int m = 8;

	uint val = 0xff;

	for (int i = 0; i < 0xfff; i++)
	{
		val--;
	}

	int different_narrative3[m + 1][n + 1];
	int i, j;

	for (i = 0; i <= m; i++)
	{
		for (j = 0; j <= n; j++)
		{
			if (i == 0 || j == 0)
				different_narrative3[i][j] = 0;

			else if (different_narrative8[i - 1] == different_narrative9[j - 1])
				different_narrative3[i][j] = different_narrative3[i - 1][j - 1] + 1;

			else
			{
				if (different_narrative3[i - 1][j] > different_narrative3[i][j - 1])
					different_narrative3[i][j] = different_narrative3[i - 1][j];
				else
					different_narrative3[i][j] = different_narrative3[i][j - 1];
			}
		}
	}
	
	assert(val == 0);
	
	return (0);
}
