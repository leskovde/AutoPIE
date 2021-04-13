import unittest
from run_tests import BaseTest

TestName = 'example8.c'

Output = '''\
'''


class Example8(BaseTest):
    def test_execution(self):
        self.runTool(TestName, Output, [])


if __name__ == '__main__':
    unittest.main()
