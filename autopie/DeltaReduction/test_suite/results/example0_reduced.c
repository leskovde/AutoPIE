void print(const char* str, int x) 
{ 
	x += 3;
	print(str, x);
}

int
main (int argc, char** argv) {
	int i = 0;
	
	

	

	print("Result: %d\n", i);
}