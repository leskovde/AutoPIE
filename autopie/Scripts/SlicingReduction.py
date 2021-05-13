#!/usr/bin/env python3

import os
import subprocess
import sys
import argparse
from types import SimpleNamespace
from slice import run_slicer
from unify import unify

parser = argparse.ArgumentParser()
parser.add_argument("--source_file", required=True, type=str, help="The name of the file in which the error occurred.")
parser.add_argument("--line_number", required=True, type=int, help="The line number on which the error occurred.")
parser.add_argument("--error_message", required=True, type=str, help="A part of the error message specifying the "
                                                                     "nature of the error.")
parser.add_argument("--arguments", type=str, default="", help="The arguments with which the program was run when the "
                                                              "error occurred.")
parser.add_argument("--reduction_ratio", type=float, default=1.0, help="Limits the reduction to a specific ratio "
                                                                       "between 0 and 1.")
parser.add_argument("-d", "--dump_dot", type=bool, default=False, help="Specifies whether a GraphViz file containing "
                                                                       "relationships of code units should be "
                                                                       "created.")
parser.add_argument("-v", "--verbose", type=bool, default=False, help="Specifies whether the tool should flood the "
                                                                      "standard output with its optional messages.")
parser.add_argument("-l", "--log", type=bool, default=False, help="Specifies whether the tool should output its "
                                                                  "optional message (with timestamps) to an external "
                                                                  "file.")
parser.add_argument("--static_slice", type=bool, default=True, help="Runs a pass of the static slicer during "
                                                                    "preprocessing. Enabled by default.")
parser.add_argument("--dynamic_slice", type=bool, default=True, help="Runs a pass of the dynamic slicer during "
                                                                     "preprocessing. Enabled by default.")
parser.add_argument("--delta", type=bool, default=True, help="Uses the minimizing Delta debugging algorithm before "
                                                             "launching naive reduction. Enabled by default.")


def run_tool(binary_path, arguments):
    print(f"Executing '{binary_path}'...")

    proc = subprocess.Popen([binary_path] + arguments)
    proc.wait()

    print("Execution done.")

    return proc.returncode


def run_static_slicer(args, var, iteration):
    static_slicer_args = SimpleNamespace(
        file=args.source_file,
        criterion=var,
        output=f"static_slice_{iteration}.txt",
        dynamic_slicer=False
    )

    run_slicer(static_slicer_args)


def run_dynamic_slicer(args, var, iteration):
    dynamic_slicer_args = SimpleNamespace(
        file=args.source_file,
        criterion=var,
        output=f"dynamic_slice_{iteration}.txt",
        arguments=args.arguments,
        dynamic_slicer=True
    )

    run_slicer(dynamic_slicer_args)


def run_delta(args):

    run_tool()


def main(args):
    source_code = args.source_file
    adjusted_line = args.line_number
    variables = []

    print(f"Extracting variables...")

    if args.static_slice:
        static_slices = []
        for var in variables:
            print(f"Running static slicing with the criterion '{var}'...")

        unified_slice = None

        print("Extracting source code for the unified slice...")

        source_code = None
        adjusted_line = None

    if args.dynamic_slice:
        dynamic_slices = []
        for var in variables:
            print(f"Running dynamic slicing with the criterion '{var}'...")

        unified_slice = None

        print("Extracting source code for the unified slice...")

        source_code = None
        adjusted_line = None

    if args.delta:
        exit_code = run_delta()

        if exit_code != 0:
            print("DeltaReduction returned a non-standard exit code. The reduction failed.")
        else:
            source_code = None

    exit_code = run_naive()

    if exit_code != 0:
        print("DeltaReduction returned a non-standard exit code. The reduction failed.")
    else:
        source_code = None

    if not save_result(source_code):
        print("The result variant could not be saved.")
        sys.exit(1)

    print("AutoPie has successfully finished.")
    sys.exit(0)


if __name__ == "__main__":
    args = parser.parse_args([] if "__file__" not in globals() else None)
    main(args)
