import unittest
from run_tests import BaseTest

TestName = 'tests/example1.c'

Output = '''
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
'''


class Example1(BaseTest):
    def test_execution(self):
        self.runTool(TestName, Output, ['--loc-file=' + TestName,
            '--loc-line=4',
            '--error-message=\"segmentation fault\"',
            '--ratio=0.5',
            '--dump-dot=true',
            '--verbose=true'])


if __name__ == '__main__':
    unittest.main()
