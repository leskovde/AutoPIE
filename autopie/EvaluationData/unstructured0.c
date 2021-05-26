/*
 * Endless recursion.
 *
 */


void print(const char* str, int x) 
{ 
	x += 3;
	print(str, x);
}

int main (int argc, char** argv) 
{
	int i = 0;
	
	const int k = 0xffff;

	for (int j = 0; j < k; j++) {
		i++;
	}

	int m = i + k;

	for (int j = 0; j < m; j++)
	{
		i += j;
	}

	print("Result: %d\n", i);
}