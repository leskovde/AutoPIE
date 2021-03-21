#!/usr/bin/env python3
import sys
import difflib
import os
import subprocess
import unittest


def create_subprocess(command, args):
    """Executes a command with given arguments and returns its return value and (stdout) output."""

    proc = subprocess.Popen([command] + args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    output, _ = proc.communicate()

    return proc.returncode, output


def strip_text(text):
    """Removes accents from a given multiline string."""

    return [line.strip() for line in text.splitlines()]


class BaseTest(unittest.TestCase):
    """Serves as a common base class for all AutoPIE tests."""

    def setUp(self):
        self.inputs_dir = 'tests'

        # TODO: Change to the actual location.
        self.executable_binary = 'build/autopie'

    def evaluate(self, expected_output, actual_output, command=''):
        """Compares the actual output to the reference output. Prints any differences and fails, if there are any."""

        expected_lines = strip_text(expected_output)
        actual_lines = strip_text(actual_output)

        if expected_lines != actual_lines:
            print(f'\nTest \'{command}\' failed.\nDiff:')
            diff = difflib.Differ().compare(expected_lines, actual_lines)
            print('\n'.join(diff))

            self.fail('Test failed.')

    def runTool(self, filename, expected_out, args):
        """Executes the AutoPIE tool with given arguments. Checks its output and compares it to the reference output."""

        input_path = os.path.join(self.inputs_dir, filename)
        return_value, actual_output = create_subprocess(self.executable_binary, args + [input_path] + ['--'])
        actual_output = actual_output.decode('utf-8')

        self.assertEqual(return_value, 0)
        self.evaluate(expected_out, actual_output, command=f'{[self.executable_binary] + args} {filename}')


def main():
    if not os.path.isdir('tests'):
        print('ERROR: Invalid directory. Run from the \'tests\' directory.')

        # if not os.path.isdir('build'):
            # print('ERROR: The tool has not been built yet. The 'build' directory does not exist.')

        sys.exit(1)
    else:
        tests = unittest.TestLoader().discover('.', 'test*.py', '.')
        result = unittest.TextTestRunner().run(tests)
        sys.exit(0 if result.wasSuccessful() else 1)


if __name__ == '__main__':
    main()