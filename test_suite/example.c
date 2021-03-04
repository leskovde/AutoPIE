void printf_mock(const char* str, int x) { x += 3; }

/* TODO: Change printf_mock for assert (or similar).
 * Basic smoke test for slicing and overall reduction - 
 * assuming the error is on the penultimate line of main (printf_mock call).
 * The result should contain only relevant declarations, expressions and statements.
 */

int
main (int argc, char** argv) {
	int i = 0;
	const int k = 0xff;

	for (int j = 0; j < k; j++) {
		i++;
	}

	long long time_waster = 0;
	const int slow = 0xffff;
	const long slower = 0xffffff;
	const long long the_slowest = 0xffffffff;

	for (int j = 0; j < slow; j++) {
		time_waster++;
	}
	
	time_waster = 0;	

	for (long j = 0; j < slower; j++) {
		time_waster++;
	}

	time_waster = 0;

	for (long long j = 0; j < the_slowest; j++) {
		time_waster++;
	}

	printf_mock("Result: %d\n", i);

	return 0;
}