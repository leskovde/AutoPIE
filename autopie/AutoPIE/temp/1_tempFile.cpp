void printf(  
{ 
	
	 x);
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