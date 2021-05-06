import unittest
from run_tests import BaseTest

TestName = 'example6.c'

Output = '''\
'''


class Example6(BaseTest):
    def test_execution(self):
        self.runTool(TestName, Output, [])


if __name__ == '__main__':
    unittest.main()
