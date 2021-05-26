#!/usr/bin/env python3

import argparse

parser = argparse.ArgumentParser()
parser.add_argument("--file", type=argparse.FileType('r'), nargs='+', help="Paths to files containing slice line "
                                                                           "numbers")
parser.add_argument("-o", "--output", action="store", type=argparse.FileType('w'), dest="output", help="The file "
                                                                                                           "path to "
                                                                                                           "which the "
                                                                                                           "result "
                                                                                                           "should be "
                                                                                                           "dumped")


def unify(args):
    print(f"Unifying {len(args.file)} slices...")

    lines = set()

    for ifs in args.file:
        for line in ifs:
            line = line.strip()
            if line.isdigit():
                lines.add(line)

        ifs.close()

    if len(lines) == 0:
        return 1

    ofs = args.output

    for line in lines:
        ofs.write(f"{line}\n")

    ofs.close()

    print(f"Done, the result has been stored in '{args.output.name}'.")

    return 0


if __name__ == "__main__":
    args = parser.parse_args([] if "__file__" not in globals() else None)
    unify(args)
