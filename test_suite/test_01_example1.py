import unittest
from run_tests import BaseTest

TestName = 'example1.c'

Output = '''\
asd

asd

asd
'''


class Example1(BaseTest):
    def test_execution(self):
        self.runTool(TestName, Output, [])


if __name__ == '__main__':
    unittest.main()
