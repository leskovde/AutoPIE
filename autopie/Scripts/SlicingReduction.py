#!/usr/bin/env python3

import sys
import argparse

parser = argparse.ArgumentParser()


def main(args):
    source_code = args.source_code
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
