#!/usr/bin/env python3
import argparse
import os
import tqdm
import itertools

import numpy as np

from pathlib import Path


class Test:
    def __init__(self, source_file, output_file):
        self.source_file_path = source_file
        self.name = Path(source_file).stem

        with open(output_file, "r") as ifs:
            self.expected_output = ifs.readlines()


parser = argparse.ArgumentParser()
parser.add_argument("--file", default="a_example", type=str, help="Input data necessary for functioning correctly")
parser.add_argument("--seed", default=42, type=int, help="Random seed")


def main(args):
    pass


if __name__ == '__main__':
    args = parser.parse_args([] if "__file__" not in globals() else None)
    main(args)