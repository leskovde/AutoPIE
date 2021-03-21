import unittest
from run_tests import BaseTest

TestName = 'example7.c'

Output = '''\
'''


class Example7(BaseTest):
    def test_execution(self):
        self.runTool(TestName, Output, [])


if __name__ == '__main__':
    unittest.main()
