import unittest
from run_tests import BaseTest

TestName = 'tests/example0.c'

Output = 'results/example0_reduced.c'

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