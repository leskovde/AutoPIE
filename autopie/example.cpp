void printf(const char* str, int x) 
{ 
	x += 3;
	printf(str, x);
}

int
main (int argc, char** argv) {
	int i = 0;
	
	const int k = 0xffff;

	for (int j = 0; j < k; j++) {
		i++;
	}

	printf("Result: %d\n", i);
}