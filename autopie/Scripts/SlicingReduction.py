#!/usr/bin/env python3

import os
import subprocess
import sys
import pathlib
import shutil
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

delta_path = "../DeltaReduction/build/bin/DeltaReduction"
naive_path = "../NaiveReduction/build/bin/NaiveReduction"
var_extractor_path = "../VariableExtractor/build/bin/VariableExtractor"
slice_extractor_path = "../SliceExtractor/build/bin/SliceExtractor"
variant_path = {
    ".c": "temp/autoPie.c",
    ".cpp": "temp/autoPie.cpp"
}

unification_output = "unification.txt"
var_extractor_output = "criteria.txt"
slice_extractor_output = {
    ".c": "slice.c",
    ".cpp": "slice.cpp"
}
adjusted_line_path = "adjustedLineNumber.txt"

language = ""


def validate_paths(args):
    valid = True

    if not os.path.exists(args.source_file):
        print("The input source file could not be found!")
        valid = False
    else:
        global language
        _, language = os.path.splitext(args.source_file)

        if not (language == ".c" or language == ".cpp"):
            print("The input file is written neither in C or C++!")
            valid = False

    if not os.path.exists(delta_path):
        print("The DeltaReduction binary is missing!")
        valid = False
    if not os.path.exists(naive_path):
        print("The NaiveReduction binary is missing!")
        valid = False
    if not os.path.exists(var_extractor_path):
        print("The VariableExtractor binary is missing!")
        valid = False
    if not os.path.exists(slice_extractor_path):
        print("The SliceExtractor binary is missing!")
        valid = False

    return valid


def run_unification(slices):
    if len(slices) < 1:
        return False

    raw_args = ["--file"]

    for slice in slices:
        raw_args.append(slice)

    raw_args.append(f"-o={unification_output}")

    unification_args = unify.parser.parse_args(raw_args)

    unify.unify(unification_args)

    return 0


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


def run_dynamic_slicer(args, variables, iteration):
    # The dynamic slicer cannot produce its output
    # if the program results in an error.
    # We need to prevent the error by making a temporary
    # change in the input - replacing the criterion
    # with a useless statement and an exit call.

    input_file = modify_criterion(args, variables, iteration)
    output_file = f"dynamic_slice_{iteration}.txt"

    dynamic_slicer_args = SimpleNamespace(
        file=input_file,
        criterion=f"{args.line_number}:whatever",
        output=output_file,
        arguments=args.arguments,
        dynamic_slicer=True
    )

    run_slicer(dynamic_slicer_args)

    return output_file


def run_tool(binary_path, arguments):
    if not os.path.exists(binary_path):
        print(f"The tool '{binary_path}' could not be found!")
        print("Make sure to build all components using a Makefile before running the script.")

    print(f"Executing '{binary_path}'...")

    proc = subprocess.Popen([binary_path] + arguments)
    proc.wait()

    print("Execution done.")

    return proc.returncode


def run_variable_extraction(args):
    var_extractor_args = [f"--loc-file={args.source_file}",
                          f"--loc-line={args.line_number}",
                          f"--verbose={args.verbose}",
                          f"--log={args.log}",
                          f"-o={var_extractor_output}",
                          args.source_file,
                          "--"
                          ]

    return run_tool(var_extractor_path, var_extractor_args)


def run_slice_extraction(args, slice_file):
    slice_extractor_args = [f"--loc-file={args.source_file}",
                            f"--slice-file={slice_file}",
                            f"--loc-line={args.line_number}",
                            f"--verbose={args.verbose}",
                            f"--log={args.log}",
                            f"-o=slice",
                            args.source_file,
                            "--"
                            ]

    return run_tool(slice_extractor_path, slice_extractor_args)


def run_delta(args):
    delta_args = [f"--loc-file={args.source_file}",
                  f"--loc-line={args.line_number}",
                  f"--error-message={args.error_message}",
                  f"--arguments={args.arguments}",
                  f"--dump-dot={args.dump_dot}",
                  f"--verbose={args.verbose}",
                  f"--log={args.log}",
                  args.source_file,
                  "--"
                  ]

    return run_tool(delta_path, delta_args)


def run_naive(args):
    naive_args = [f"--loc-file={args.source_file}",
                  f"--loc-line={args.line_number}",
                  f"--error-message={args.error_message}",
                  f"--arguments={args.arguments}",
                  f"--ratio={args.reduction_ratio}",
                  f"--dump-dot={args.dump_dot}",
                  f"--verbose={args.verbose}",
                  f"--log={args.log}",  # TODO: Logging twice in a row will remove the previous log. Add a cool-down.
                  args.source_file,
                  "--"
                  ]

    return run_tool(naive_path, naive_args)


def get_variables_on_line(args, file_path):
    variables = []

    if not run_variable_extraction(args):
        with open(file_path, "r") as file:
            for line in file:
                variables.append(line.strip())

    return variables


def get_extracted_slice(args, slice_extractor_output):
    global language

    extracted_file = pathlib.Path(slice_extractor_output[language])

    if not extracted_file.exists():
        return args.source_file

    return slice_extractor_output[language]


def get_adjusted_line(args, file_path):
    with open(file_path, "r") as file:
        line = file.read().strip()

        if line.isdigit():
            return int(line)

    return args.line_number


def update_source_from_slices(args, dynamic_slices):
    if not run_unification(dynamic_slices):
        print("Extracting source code for the unified slice...")

        if not run_slice_extraction(args, unification_output):
            args.source_file = get_extracted_slice(args, slice_extractor_output)
            args.line_number = get_adjusted_line(args, adjusted_line_path)


def update_source_from_reduction(args):
    reduced_file = pathlib.Path(variant_path[language])

    if not reduced_file.exists():
        return

    # Copy the result to the current directory.
    args.source_file = shutil.copy2(variant_path[language], ".")


def modify_criterion(args, variables, iteration):
    with open(args.source_file, "r") as ifs:
        file_contents = ifs.readlines()

    exit_header = "<stdlib.h>" if language == ".c" else "<cstdlib>"
    file_contents.insert(0, f"#include {exit_header}\n")

    new_criterion = ""

    for var in variables:
        new_criterion += f"(void){var.split(':')[1]}; "

    new_criterion += "\n"
    file_contents.insert(args.line_number - 1, new_criterion)

    file_path = f"dynamic_{iteration}{language}"

    with open(file_path, "w") as ofs:
        ofs.write("".join(file_contents))

    return file_path


def save_result(file_path):
    output_file = pathlib.Path(variant_path[language])

    if not output_file.exists():
        return False

    # Copy the result to the current directory.
    shutil.copy2(variant_path[language], ".")

    return True


def main(args):
    args.line_number = str(args.line_number)
    args.reduction_ratio = str(args.reduction_ratio)
    args.dump_dot = "true" if args.dump_dot else "false"
    args.verbose = "true" if args.verbose else "false"
    args.log = "true" if args.log else "false"

    validate_paths(args)

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
        i = 0

        print(f"Running dynamic slicing with the criterion '{args.line_number}'...")

        dynamic_slice = run_dynamic_slicer(args, variables, i)

        update_source_from_slices(args, [dynamic_slice])

    if args.delta:
        exit_code = run_delta(args)

        if exit_code != 0:
            print("DeltaReduction returned a non-standard exit code. The reduction failed.")
        else:
            update_source_from_reduction(args)

    exit_code = run_naive(args)

    if exit_code != 0:
        print("DeltaReduction returned a non-standard exit code. The reduction failed.")
    else:
        update_source_from_reduction(args)

    if not save_result(args.source_file):
        print("The result variant could not be saved.")
        sys.exit(1)

    print("AutoPie has successfully finished.")
    sys.exit(0)


if __name__ == "__main__":
    args = parser.parse_args([] if "__file__" not in globals() else None)
    main(args)
