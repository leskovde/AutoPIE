#!/usr/bin/env bash

echo "This script will build LLVM from source."
echo "Building LLVM, Clang, and LLDB can be difficult for newbies."
echo "Therefore, some work is taken care of by this script."
echo -e "\nFollowing commands will be used:"
echo -e "\t\twget to download the LLVM source"
echo -e "\t\ttar with xz-utils"
echo -e "\t\tmktemp to create a temporary directory"
echo -e "\t\tmkdir to create a build subdirectory in the temp directory"
echo -e "\t\trm to remove the temp directory"

if [ "$EUID" -ne 0 ]; then 
	echo -e "\nThe ninja install command requires root priviledges in order to function correctly."
	echo "It is required that you run this script as root."
	exit 1
fi

read -p "Do you want to continue (y/n)?" choice
case "$choice" in
  y|Y ) 
	  echo "Continuing...";;
  n|N ) 
	  echo "Terminating..."
	  exit 1;;
  * ) 
	  echo "Invalid. Terminating..."
	  exit 1;;
esac

echo "Downloading LLVM 11.0.0. ..."
wget https://github.com/llvm/llvm-project/releases/download/llvmorg-11.0.0/llvm-project-11.0.0.tar.xz

if [ $? -ne 0 ]; then
        echo "The download failed. Terminating..."
        cd -
        exit 1
fi

temp_dir=$(mktemp -d -t llvm-XXXX)
echo "Temp dir $temp_dir created."

echo "Extracting the downloaded archive..."
tar -xf llvm-project-11.0.0.tar.xz -C $temp_dir

echo "Removing the downloaded archive..."
rm -f llvm-project-11.0.0.tar.xz

echo "Creating the build subdirectory..."
mkdir $temp_dir/llvm-project-11.0.0/build

echo "Switching to the build subdirectory..."
cd $temp_dir/llvm-project-11.0.0/build

echo "Running CMake..."
cmake -G Ninja ../llvm -DLLVM_ENABLE_PROJECTS="clang;lldb" -DLLVM_TARGETS_TO_BUILD=X86 -DLLVM_USE_LINKER=gold -DLLVM_PARALLEL_LINK_JOBS=1 -DLLVM_USE_SPLIT_DWARF=ON -DCMAKE_BUILD_TYPE=Release -DLLDB_INCLUDE_TESTS=OFF -DLLDB_ENABLE_PYTHON=1

if [ $? -ne 0 ]; then
	echo "The CMake command failed. Terminating..."
	cd -
	exit 1
fi

echo "Running ninja build..."
ninja

if [ $? -ne 0 ]; then
        echo "The ninja build failed. Terminating..."
        cd -
        exit 1
fi

echo "Running ninja install..."
ninja install

if [ $? -ne 0 ]; then
        echo "The ninja install command failed. Terminating..."
        cd -
        exit 1
fi

echo "Exporting a new linker library path..."
export LD_LIBRARY_PATH="/usr/local/lib/:$LD_LIBRARY_PATH"

echo "Switching back to the previous directory..."
cd -

echo "Removing the temp dir $temp_dir ..."
rm -rf $temp_dir

echo "Done."
exit 0
