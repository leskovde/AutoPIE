#!/usr/bin/env python3

import os
import docker
import argparse
import shutil

parser = argparse.ArgumentParser()
parser.add_argument("--file", required=True, type=str, help="The source file to be sliced")
parser.add_argument("--criterion", required=True, type=str, help="The slicing criterion")
parser.add_argument("-o", "--output", default="output.txt", type=str, help="A series of line numbers contained in the "
                                                                           "slice")
parser.add_argument("--arguments", default="", type=str, help="Arguments for the program trace")
parser.add_argument("--dynamic", dest="dynamic_slicer", action="store_true",
                    help="Use the dynamic slicer instead of the static slicer")
parser.add_argument("--static", dest="dynamic_slicer", action="store_false",
                    help="Use the static slicer instead of the dynamic slicer")
parser.set_defaults(dynamic_slicer=False)


def slice_dg(args):
    # Runs the DG slicer using a Docker container.
    # During the process, a temporary directory is
    # created and mounted to the container.
    # The result is checked and moved from the docker
    # temp path to the desired output path.

    # Create the temp path and copy the input to it.

    if not os.path.exists("./dg-data"):
        os.mkdir("./dg-data")

    vol_path = os.path.abspath("./dg-data")

    print("Copying the input file to the shared medium...")
    shutil.copy2(args.file, vol_path)

    print("Initializing docker...")
    client = docker.from_env()

    print("Running the docker...")

    # Prepare the commands to be executed in the docker.

    compiler_cmd = f"clang{args.cpp} -emit-llvm -c -g /data-mapped/" + args.file + " -o _code.bc"

    slicer_path = "/opt/dg/tools/llvm-slicer"
    slicer_cmd = slicer_path + " -c " + args.criterion + " _code.bc"

    source_converter_cmd = "/opt/dg/tools/llvm-to-source _code.sliced > /data-mapped/" + args.output

    cmd = [
        "-c",
        compiler_cmd + " && " + slicer_cmd + " && " + source_converter_cmd
    ]

    # Map the temp path.

    volumes = {
        vol_path: {
            "bind": "/data-mapped",
            "mode": "rw",
        },
    }

    # Run the container.

    container = client.containers.run(
        image="mchalupa/dg",
        command=cmd,
        volumes=volumes,
        detach=True
    )

    container.wait()

    print("Docker exited...")

    # Process the results.

    print("Checking and moving the result...")
    if os.path.exists(os.path.join(vol_path, args.output)):
        shutil.move(os.path.join(vol_path, args.output), args.output)
        print(container.logs().decode("utf-8"))

    container.remove()


def slice_giri(args):
    # Runs the DG slicer using a Docker container.
    # During the process, a temporary directory is
    # created and mounted to the container.
    # The result is checked and moved from the docker
    # temp path to the desired output path.

    # Create the temp path and copy the input to it.

    if not os.path.exists("./giri-data"):
        os.mkdir("./giri-data")

    vol_path = os.path.abspath("./giri-data")

    print("Copying the input file to the shared medium...")
    shutil.copy2(args.file, vol_path)

    print("Initializing docker...")
    client = docker.from_env()

    print("Running the docker...")

    # Prepare the commands to be executed in the docker.

    directory = "/giri/test/UnitTests/temp/"

    slicer_cmd = "make -C " + directory

    makefile_content = "'NAME = " + args.file.split(".")[0] + "\nLDFLAGS = -lm\nINPUT ?= " + \
                       args.arguments + "\nCRITERION ?= -criterion-loc=criterion-loc.txt\nMAPPING ?= " + \
                       "-mapping-function=main\ninclude ../../Makefile.common'"

    slicer_create_makefile_cmd = "mkdir " + directory + \
                                 " && echo " + args.file + " " + args.criterion.split(":")[0] + " > " + directory + \
                                 "criterion-loc.txt" + " && cp /data-mapped/" + args.file + " " + directory + \
                                 " && echo " + makefile_content + " > " + directory + "Makefile"

    source_converter_cmd = "cat " + directory + args.file.split(".")[0] + ".slice.loc > " + \
                           "/data-mapped/" + args.output

    cmd = [
        "/bin/bash",
        "-c",
        slicer_create_makefile_cmd + " && " + slicer_cmd + " && " + source_converter_cmd
    ]

    # Map the temp path.

    volumes = {
        vol_path: {
            "bind": "/data-mapped",
            "mode": "rw",
        },
    }

    # Run the container.

    container = client.containers.run(
        image="liuml07/giri",
        command=cmd,
        volumes=volumes,
        detach=True
    )

    container.wait()

    print("Docker exited...")

    # Process the results.

    print("Checking and moving the result...")
    if os.path.exists(os.path.join(vol_path, args.output)):
        shutil.move(os.path.join(vol_path, args.output), args.output)
        print(container.logs().decode("utf-8"))

    container.remove()


def run_slicer(args):
    if args.file is None:
        print("Invalid usage: the --file argument is empty.")

        return

    if args.criterion is None:
        print("Invalid usage: the --criterion argument is empty.")

        return

    if args.dynamic_slicer:
        print("RUNNING GIRI SLICER...")
        slice_giri(args)
        message = "Done! Check 'giri-data/" + args.output + "'to see the result."

    else:
        print("RUNNING DG SLICER...")
        slice_dg(args)
        message = "Done! Check 'dg-data/" + args.output + "' to see the result."

    print(message)


if __name__ == "__main__":
    args = parser.parse_args([] if "__file__" not in globals() else None)
    run_slicer(args)
