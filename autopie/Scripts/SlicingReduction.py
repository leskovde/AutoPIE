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
parser.add_argument("-d", "--dump_dot", type=lambda x: (str(x).lower() in ['true', '1', 'yes']), default=False,
                    help="Specifies whether a GraphViz file containing "
                         "relationships of code units should be "
                         "created.")
parser.add_argument("-v", "--verbose", type=lambda x: (str(x).lower() in ['true', '1', 'yes']), default=False,
                    help="Specifies whether the tool should flood the "
                         "standard output with its optional messages.")
parser.add_argument("-l", "--log", type=lambda x: (str(x).lower() in ['true', '1', 'yes']), default=False,
                    help="Specifies whether the tool should output its "
                         "optional message (with timestamps) to an external "
                         "file.")
parser.add_argument("--static_slice", type=lambda x: (str(x).lower() in ['true', '1', 'yes']), default=True,
                    help="Runs a pass of the static slicer during "
                         "preprocessing. Enabled by default.")
parser.add_argument("--dynamic_slice", type=lambda x: (str(x).lower() in ['true', '1', 'yes']), default=True,
                    help="Runs a pass of the dynamic slicer during "
                         "preprocessing. Enabled by default.")
parser.add_argument("--delta", type=lambda x: (str(x).lower() in ['true', '1', 'yes']), default=True,
                    help="Uses the minimizing Delta debugging algorithm before "
                         "launching naive reduction. Enabled by default.")
parser.add_argument("--inject", type=lambda x: (str(x).lower() in ['true', '1', 'yes']), default=False,
                    help="Instead of calling the debugged program with its "
                         "arguments, the tool specifies the arguments directly "
                         "in source code.")

# Define relative paths to all components.

delta_path = "../DeltaReduction/build/bin/DeltaReduction"
naive_path = "../NaiveReduction/build/bin/NaiveReduction"
var_extractor_path = "../VariableExtractor/build/bin/VariableExtractor"
slice_extractor_path = "../SliceExtractor/build/bin/SliceExtractor"

# Define paths to output files

variant_path = {
    ".c": "temp/autoPieOut.c",
    ".cpp": "temp/autoPieOut.cpp"
}
unification_output = "unification.txt"
var_extractor_output = "criteria.txt"
slice_extractor_output = {
    ".c": "slice.c",
    ".cpp": "slice.cpp"
}
adjusted_line_path = "adjustedLineNumber.txt"

# The language in which the source code is written-
# determined based on the file extension.
language = ""

# A list of created resource for further cleanup.
created_files = []


def validate_paths(args):
    # Checks whether all required paths are present.
    # Returns False if any path is missing.
    # Additionally, initializes the `language` variable.

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


def check_and_remove(file_path):
    # Safely and silently removes a file
    # from a provided file path.

    if os.path.exists(file_path):
        try:
            os.remove(file_path)
        except OSError:
            pass


def run_unification(slices):
    # Executes the unification script with
    # the generated slice files (containing
    # line numbers).
    # Saves the output in the path specified
    # in global variables.

    if len(slices) < 1:
        return 1

    check_and_remove(unification_output)

    raw_args = ["--file"]

    for slice in slices:
        raw_args.append(slice)

    raw_args.append(f"-o={unification_output}")

    unification_args = unify.parser.parse_args(raw_args)

    global created_files
    created_files.append(unification_output)

    return unify.unify(unification_args)


def run_static_slicer(args, var, iteration):
    # Executes the static slicer script for
    # a given criterion.
    # The result is saved in file with
    # the following name:
    # static_slice_`iteration`.txt

    check_and_remove(f"./slice_input{language}")

    input_file = shutil.copy2(args.source_file, f"./slice_input{language}")
    output_file = f"static_slice_{iteration}.txt"

    check_and_remove(output_file)

    criterion = var
    main_loc = 1

    # Handle argument injection.
    if args.inject and args.arguments != "":
        # Find the start of the main function,
        # insert two lines that specify argv and
        # argc, and adjust all modified line locations.

        parts = criterion.split(":")
        adjusted_line = int(parts[0])

        found = False

        with open(input_file, "r") as ifs:
            for line in ifs:
                main_loc += 1

                if found:
                    if "{" in line:
                        break

                if "main(" in line or "main (" in line:
                    if "{" in line:
                        break
                    found = True

        # Add two lines into the file.

        inject_arguments(args.arguments, input_file, main_loc)

        adjusted_line += (0 if adjusted_line <= main_loc else 2)

        # Adjust the criterion.

        criterion = f"{adjusted_line}:{parts[1]}"

    static_slicer_args = SimpleNamespace(
        file=input_file,
        criterion=criterion,
        output=output_file,
        cpp="" if language == ".c" else "++",
        dynamic_slicer=False
    )

    run_slicer(static_slicer_args)

    # If the injection took place, some
    # line numbers need adjustments.
    if args.inject:
        adjust_slice(output_file, main_loc)

    global created_files
    created_files.append(input_file)
    created_files.append(output_file)

    return output_file


def inject_arguments(arguments, file_path, location):
    # Inserts two lines with argv and argc
    # into the content of a specified file.
    # The file is then dumped into a specified
    # location, the original file remains
    # unmodified.

    with open(file_path, "r") as ifs:
        file_contents = ifs.readlines()

    words = arguments.strip("\"").split()

    argv = 'const char* n_argv[] = { "program", '

    # Append all arguments.
    for word in words:
        argv += f'"{word}", '

    argv += "};\n"
    argc = f"argc = {len(words) + 1}; argv = n_argv;\n"

    file_contents.insert(location - 1, argv)
    file_contents.insert(location, argc)

    with open(file_path, "w") as ofs:
        ofs.write("".join(file_contents))


def run_dynamic_slicer(args, variables, iteration):
    # The dynamic slicer cannot produce its output
    # if the program results in an error.
    # We need to prevent the error by making a temporary
    # change in the input - replacing the criterion
    # with a useless statement and an exit call.

    input_file = modify_criterion(args, variables, iteration)
    output_file = f"dynamic_slice_{iteration}.txt"

    check_and_remove(output_file)

    adjusted_line_number = str((int(args.line_number) + 1))

    dynamic_slicer_args = SimpleNamespace(
        file=input_file,
        criterion=f"{adjusted_line_number}:whatever",
        output=output_file,
        arguments=args.arguments,
        dynamic_slicer=True
    )

    run_slicer(dynamic_slicer_args)

    adjust_slice(output_file, 1)

    global created_files
    created_files.append(output_file)

    return output_file


def adjust_slice(output_file, start):
    # Adjust all line numbers after
    # a given line number by two.
    # This is a helper function
    # for argument injection.

    lines = []
    if os.path.exists(output_file):
        with open(output_file, "r") as ifs:
            for line in ifs:
                line = line.strip()
                if line.isdigit():
                    if int(line) >= start:
                        lines.append(int(line) - 2)
                    else:
                        lines.append(line)

        with open(output_file, "w") as ofs:
            for line in lines:
                ofs.write(f"{line}\n")


def run_tool(binary_path, arguments):
    # Runs a generic tool by executing
    # a subprocess.
    # Requires the path to the binary
    # and a list of its arguments.
    # Returns the exit code of tool.

    if not os.path.exists(binary_path):
        print(f"The tool '{binary_path}' could not be found!")
        print("Make sure to build all components using a Makefile before running the script.")

    print(f"Executing '{binary_path}'...")

    proc = subprocess.Popen([binary_path] + arguments)
    proc.wait()

    print("Execution done.")

    return proc.returncode


def run_variable_extraction(args):
    # Runs the VariableExtractor component.
    # Variables are extracted based on
    # the current values in args.
    # The result is saved to the path
    # specified in the global variables.
    # Returns the exit code of tool.

    check_and_remove(var_extractor_output)

    var_extractor_args = [f"--loc-line={args.line_number}",
                          f"--verbose={args.verbose}",
                          f"--log={args.log}",
                          f"-o={var_extractor_output}",
                          args.source_file,
                          "--"
                          ]

    return run_tool(var_extractor_path, var_extractor_args)


def run_slice_extraction(args, slice_file):
    # Runs the SliceExtractor component.
    # Slices are extracted based on the values
    # in args and on the specified file
    # of line numbers.
    # The result is saved to two paths
    # specified in the global variables.
    # Returns the exit code of tool.

    check_and_remove(slice_extractor_output[language])
    check_and_remove(adjusted_line_path)

    slice_extractor_args = [f"--slice-file={slice_file}",
                            f"--loc-line={args.line_number}",
                            f"--verbose={args.verbose}",
                            f"--log={args.log}",
                            f"-o=slice",
                            args.source_file,
                            "--"
                            ]

    return run_tool(slice_extractor_path, slice_extractor_args)


def run_delta(args):
    # Runs the DeltaReduction component.
    # Values in args are passed directly
    # to the component.
    # The result is saved to the path
    # specified in the global variables.
    # Returns the exit code of tool.

    check_and_remove(f"autoPieOut{language}")

    delta_args = [f"--loc-line={args.line_number}",
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
    # Runs the NaiveReduction component.
    # Values in args are passed directly
    # to the component.
    # The result is saved to the path
    # specified in the global variables.
    # Returns the exit code of tool.

    naive_args = [f"--loc-line={args.line_number}",
                  f"--error-message={args.error_message}",
                  f"--arguments={args.arguments}",
                  f"--ratio={args.reduction_ratio}",
                  f"--dump-dot={args.dump_dot}",
                  f"--verbose={args.verbose}",
                  f"--log={args.log}",
                  args.source_file,
                  "--"
                  ]

    return run_tool(naive_path, naive_args)


def get_variables_on_line(args, file_path):
    # Wrapper for executing
    # the VariableExtractor component
    # and loading its result into
    # memory.
    # Returns a list of criteria.

    variables = []

    if not run_variable_extraction(args):
        with open(file_path, "r") as ifs:
            for line in ifs:
                variables.append(line.strip())

    global created_files
    created_files.append(file_path)

    return variables


def get_extracted_slice(args):
    # Wrapper for executing
    # the SliceExtractor component
    # and loading its result into
    # memory.
    # Returns a path to the slice
    # source file.

    global language

    extracted_file = pathlib.Path(slice_extractor_output[language])

    if not extracted_file.exists():
        return args.source_file

    global created_files
    created_files.append(slice_extractor_output[language])
    created_files.append(adjusted_line_path)

    return slice_extractor_output[language]


def get_adjusted_line(args, file_path):
    # Reads the adjusted line from
    # the SliceExtractor tool.
    # If successful, returns
    # the read line as int.
    # Otherwise, returns
    # the existing line number.

    with open(file_path, "r") as ifs:
        line = ifs.read().strip()

        if line.isdigit():
            return int(line)

    global created_files
    created_files.append(file_path)

    return args.line_number


def update_source_from_slices(args, slices):
    # Updates the args.source_file
    # value based on the output of
    # slicers.
    # Handles the case in which
    # slicers, unification, or
    # slice extraction fails
    # by keeping the old value.

    available_slices = []
    for slice in slices:
        if os.path.exists(slice):
            available_slices.append(slice)

    if not run_unification(available_slices):
        print("Extracting source code for the unified slice...")

        if not run_slice_extraction(args, unification_output):
            args.source_file = get_extracted_slice(args)
            args.line_number = get_adjusted_line(args, adjusted_line_path)

    global created_files
    for slice in available_slices:
        created_files.append(slice)


def update_source_from_reduction(args):
    # Updates the args.source_file
    # value based on the output of
    # Naive or Delta Reductions.
    # Handles the case in which
    # the reduction fails
    # by keeping the old value.

    reduced_file = pathlib.Path(variant_path[language])

    if not reduced_file.exists():
        return

    # Copy the result to the current directory.
    args.source_file = shutil.copy2(variant_path[language], ".")

    global created_files
    created_files.append(args.source_file)
    created_files.append(variant_path[language])


def modify_criterion(args, variables, iteration):
    # Due to dynamic slicer's limitations,
    # this functions inserts code that
    # prevents the debugged program
    # from failing on a given line.
    # It does so by inserting
    # a void cast for all variables
    # in the criterion (thus preserving
    # the criterion) and calling
    # _Exit(0).
    # Appropriate includes are
    # also inserted.

    with open(args.source_file, "r") as ifs:
        file_contents = ifs.readlines()

    exit_header = "<stdlib.h>" if language == ".c" else "<cstdlib>"
    file_contents.insert(0, f"#include {exit_header}\n")

    new_criterion = ""

    for var in variables:
        new_criterion += f"(void){var.split(':')[1]}; "

    new_criterion += "\n"
    file_contents.insert(int(args.line_number), new_criterion)

    exit_command = "_Exit(0);\n" if language == ".c" else "std::exit(0);\n"
    file_contents.insert(int(args.line_number) + 1, exit_command)

    file_path = f"dynamic_{iteration}{language}"

    with open(file_path, "w") as ofs:
        ofs.write("".join(file_contents))

    global created_files
    created_files.append(file_path)

    return file_path


def save_result(args):
    # Copies the results of
    # the entire script to
    # the standard output
    # location:
    # ./autoPieOut`language`

    output_file = pathlib.Path(variant_path[language])

    if not output_file.exists():
        try:
            shutil.copy2(args.source_file, f"autoPieOut{language}")
        except Exception as e:
            pass

        return False

    # Copy the result to the current directory.
    try:
        shutil.copy2(variant_path[language], ".")
    except Exception as e:
        pass

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

    # Run the static slicer first, unify its results,
    # update the current debugged file.

    if args.static_slice:
        static_slices = []
        i = 0

        for var in variables:
            print(f"Running static slicing with the criterion '{var}'...")

            static_slices.append(run_static_slicer(args, var, i))
            i += 1

        update_source_from_slices(args, static_slices)

    # The attempt to run the dynamic slicer, unify
    # its results, update the current debugged file.

    if args.dynamic_slice:
        i = 0

        print(f"Running dynamic slicing with the criterion '{args.line_number}'...")

        dynamic_slice = run_dynamic_slicer(args, variables, i)

        update_source_from_slices(args, [dynamic_slice])

    # Run Delta on the current intermediate result.
    # If successful, update the intermediate result.

    if args.delta:
        exit_code = run_delta(args)

        if exit_code != 0:
            print("DeltaReduction returned a non-standard exit code. The reduction failed.")
        else:
            update_source_from_reduction(args)

    # Finally, run Naive on the current intermediate
    # result.
    # If successful, update the intermediate result.

    exit_code = run_naive(args)

    if exit_code != 0:
        print("NaiveReduction returned a non-standard exit code. The reduction failed.")
    else:
        update_source_from_reduction(args)

    # Save whatever intermediate result there is.

    if not save_result(args):
        print("NaiveReduction could not find a smaller variant - reverting the result of the previous step...")

    print("AutoPie has successfully finished.")
    sys.exit(0)


if __name__ == "__main__":
    args = parser.parse_args([] if "__file__" not in globals() else None)

    try:
        main(args)
    finally:
        for file in created_files:
            check_and_remove(file)
        if os.path.exists("dg-data"):
            shutil.rmtree("dg-data")
        if os.path.exists("giri-data"):
            shutil.rmtree("giri-data")
