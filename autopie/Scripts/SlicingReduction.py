#!/usr/bin/env python3

import os
import subprocess
import sys
import argparse
from types import SimpleNamespace

import unify
from slice import run_slicer

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

# TODO: Validate that the tools exist.
delta_path = "../DeltaReduction/build/bin/DeltaReduction"
naive_path = "../NaiveReduction/build/bin/NaiveReduction"
var_extractor_path = "../VariableExtractor/build/bin/VariableExtractor"
slice_extractor_path = "../SliceExtractor/build/bin/SliceExtractor"

unification_output = "unification.txt"
var_extractor_output = "criteria.txt"
slice_extractor_output = ["slice.c", "slice.cpp"]   # TODO: Make SliceExtractor return valid extension.
adjusted_line_path = ""


def run_unification(slices):
    if len(slices) < 1:
        return False

    raw_args = ["--file"]

    for slice in slices:
        raw_args.append(slice)

    raw_args.append("-o=")
    raw_args.append(unification_output)

    unification_args = unify.parser.parse_args(raw_args)

    unify.unify(unification_args)

    return True


def run_static_slicer(args, var, iteration):
    output_file = f"static_slice_{iteration}.txt"

    static_slicer_args = SimpleNamespace(
        file=args.source_file,
        criterion=var,
        output=output_file,
        dynamic_slicer=False
    )

    run_slicer(static_slicer_args)

    return output_file


def run_dynamic_slicer(args, var, iteration):
    output_file = f"dynamic_slice_{iteration}.txt"

    dynamic_slicer_args = SimpleNamespace(
        file=args.source_file,
        criterion=var,
        output=output_file,
        arguments=args.arguments,
        dynamic_slicer=True
    )

    run_slicer(dynamic_slicer_args)

    return output_file


def run_tool(binary_path, arguments):
    print(f"Executing '{binary_path}'...")

    proc = subprocess.Popen([binary_path] + arguments)
    proc.wait()

    print("Execution done.")

    return proc.returncode


def run_variable_extraction(args):
    var_extractor_args = ["--loc-file=", args.source_file,
                          "--loc-line=", args.line_number,
                          "--verbose=", args.verbose,
                          "--log=", args.log,
                          "-o=", var_extractor_output
                          ]

    return run_tool(var_extractor_path, var_extractor_args)


def run_slice_extraction(args, slice_file):
    slice_extractor_args = ["--loc-file=", args.source_file,
                            "--slice-file", slice_file,
                            "--verbose=", args.verbose,
                            "--log=", args.log,
                            "-o=", slice_extractor_output
                            ]

    return run_tool(slice_extractor_path, slice_extractor_args)


def run_delta(args):
    delta_args = ["--loc-file=", args.source_file,
                  "--loc-line=", args.line_number,
                  "--error-message=", args.error_message,
                  "--arguments=", args.arguments,
                  "--dump-dot=", args.dump_dot,
                  "--verbose=", args.verbose,
                  "--log=", args.log
                  ]

    return run_tool(delta_path, delta_args)


def run_naive(args):
    naive_args = ["--loc-file=", args.source_file,
                  "--loc-line=", args.line_number,
                  "--error-message=", args.error_message,
                  "--arguments=", args.arguments,
                  "--ratio=", args.reduction_ratio,
                  "--dump-dot=", args.dump_dot,
                  "--verbose=", args.verbose,
                  "--log=", args.log  # TODO: Logging twice in a row will remove the previous log. Add a cool-down.
                  ]

    return run_tool(naive_path, naive_args)


def get_variables_on_line(args, file_path):
    variables = []

    if run_variable_extraction(args):
        with open(file_path, "r") as file:
            for line in file:
                variables.append(line.strip())

    return variables


def get_extracted_slice(file_path):

    return ""


def get_adjusted_line(file_path):
    with open(file_path, "r") as file:
        line = file.read().strip()

        if line.isdigit():
            return int(line)

    return args.line_number


def update_source_from_slices(args, dynamic_slices):
    if run_unification(dynamic_slices):
        print("Extracting source code for the unified slice...")

        if run_slice_extraction(unification_output):
            args.source_file = get_extracted_slice(slice_extractor_output)
            args.line_number = get_adjusted_line(adjusted_line_path)


def save_result(file_path):
    
    return True


def main(args):

    print(f"Extracting variables...")
    variables = get_variables_on_line(args, var_extractor_output)

    if args.static_slice:
        static_slices = []
        i = 0

        for var in variables:
            print(f"Running static slicing with the criterion '{var}'...")

            static_slices.append(run_static_slicer(args, var, i))
            i += 1

        update_source_from_slices(args, static_slices)

    if args.dynamic_slice:
        dynamic_slices = []
        i = 0

        for var in variables:
            print(f"Running dynamic slicing with the criterion '{var}'...")

            dynamic_slices.append(run_dynamic_slicer(args, var, i))
            i += 1

        update_source_from_slices(args, dynamic_slices)

    if args.delta:
        exit_code = run_delta(args)

        if exit_code != 0:
            print("DeltaReduction returned a non-standard exit code. The reduction failed.")
        else:
            source_code = None

    exit_code = run_naive(args)

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
