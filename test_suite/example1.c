#include <stdio.h>
#include <stdlib.h>

/*
 * Caused by a stray ";" on line 36. Accidental bugs are often caused by stray
 * characters, etc. While "minor" in their fix, they can be the devil to find! 
 *
 */

int
main()
{
	int i, j;
	int numrows = 5;
	int numcols = 7;
	int pixels;

	int different_narrative_00 = 0;
	int different_narrative_01 = 81;
	int different_narrative_02 = 0, different_narrative_03 = 1;
	int different_narrative_04, different_narrative_05;
	
	if (different_narrative_01 == 0)
	{
		different_narrative_00 = 0;
	}
	
	for (different_narrative_05 = 2; different_narrative_05 <= different_narrative_01; different_narrative_05++)
	{
		different_narrative_04 = different_narrative_02 + different_narrative_03;
		different_narrative_02 = different_narrative_03;
		different_narrative_03 = different_narrative_04;
	}

	for (i=0; i<numrows; i++)
  		for (j=0; j<numcols; j++);
			pixels++;
	
	if (different_narrative_01 != 0)
	{
		different_narrative_00 = different_narrative_03;
	}
	
	assert(pixels == numrows * numcols);

	return (0);
}
