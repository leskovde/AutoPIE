# AutoPIE

*Automated Problem-Investigating Execution*

<img src="img\logo.png" alt="logo" style="zoom: 67%;" />

## About

AutoPIE is a tool for program reduction and minimization with respect to a given runtime error. The tool was created as a part of a bachelor thesis in 2021. The goal is to reduce the size of the given source code while preserving the root cause of the desired runtime error in the code. This prototype implementation supports the reduction of source code written in C and C++.

Since the current version is used mainly for evaluation of different reduction and minimization methods, it does not implement all intended features. Moreover, it does not support any platforms outside the latest mainstream Linux distributions (e.g., Manjaro 21.0, Ubuntu 20.04). The status of the development can be seen in the matrix below.

| Feature                      | Status             |
| ---------------------------- | ------------------ |
| Exponential minimization     | :heavy_check_mark: |
| Polynomial reduction         | :heavy_check_mark: |
| Slicing-based pre-processing | :heavy_check_mark: |
| Visualization                | :heavy_check_mark: |
| Validation for Windows       | :x:                |
| Visual Studio Code plugin    | :x:                |



## Example

The following C++ program results in the *segmentation fault* error upon execution.

```c++
#include <iostream>

/*
 * An example from the text. An endless recursion in C++.
 */

long get_factorial(int n)
{
	// Missing the stopping 
	// constraint 
	// => segmentation fault.
	return (n * get_factorial(n - 1));
}

int main()
{
	const int n = 20;
	long loop_result = 1;
	
	for (int i = 1; i <= n; 
		i++)
	{
		loop_result *= i;
	}
	
	long recursive_result = get_factorial(n);
	
	if (loop_result != recursive_result)
	{
		std::cout << loop_result << "\n"; 
		std::cout << recursive_result << "\n";
			
		return (1);
	}
	
	std::cout << "Success.\n";
	
	return (0);
}
```

Upon compiling and launching the program, we discover that the error arises on line *12*, i.e., the `return (n * get_factorial(n - 1));`  statement. By supplying AutoPIE with the source code, the line number, and the error message, we get the following minimized source code variant.

```c++
#include <iostream>

/*
 * An example from the text. An endless recursion in C++.
 */

long get_factorial(int n)
{
	// Missing the stopping 
	// constraint 
	// => segmentation fault.
	return (n * get_factorial(n - 1));
}

int main()
{
	const int n = 20;
	
	long recursive_result = get_factorial(n);
}
```

This variant results in the same error, while narrowing down the code containing the error's root cause to its minimal size.

## Requirements

The project uses LLVM (11.0.0), Clang LibTooling, and LLDB. These prerequisites must be built from source and thus require the following packages and tools.

- python2
- python3
- python-dev
- libedit-dev
- cmake
- ninja-build
- make
- gcc
- zlib
- coreutils
- graphviz
- docker
- docker-py
- swig
- doxygen
- docker (Python 3 package)
- argparse (Python 3 package)
- shutil (Python 3 package)
- pathlib (Python 3 package)

Note that the installation of the Docker Python API might require additional steps on some systems. Please refer to https://docs.docker.com/engine/api/sdk/#install-the-sdks for more detailed information.

Building the LLVM project with its proper configuration can be simplified by launching the `buildLLVM.sh` script. The script automatically downloads, extracts, and installs the LLVM project version 11.0.0. However, the user must first install all the listed packages and tools.

## Building AutoPIE

The AutoPIE project can be built by invoking `make` in the AutoPie directory. This Makefile also features a target for building global documentation. However, if only an individual component or documentation is required to be built, the user can do so by invoking `make` in the components directory. All such components reside in *Common*, *DeltaReduction*, *NaiveReduction*, *SliceExtractor*, and *VariableExtractor* directories. 

The binaries are generated in the `<component's directory>/build/bin/` path, while the documentation is dumped to `<component's directory>/docs/`. The global documentation, i.e., the documentation for all existing components, is generated to `AutoPie/docs/`.

## Running AutoPIE

The user can either run each component individually by executing the generated binary in the `<component's directory>/build/bin/` directory. The necessary arguments and their usage can be seen by supplying the `--help` option when launching the binary. This is the desired way of launching the *NaiveReduction* and the *DeltaReduction* algorithms.

Alternatively, the user can run the slicing-based algorithm by launching the `Scripts/SlicingReduction.py` script. The script requires all project's components to be built and available, as well as a working Docker Python API. The script uses the Docker images of two existing slicer projects, both available on GitHub. The static slicer is available on [mchalupa/dg](https://github.com/mchalupa/dg) and the dynamic slicer on [liuml07/giri](https://github.com/liuml07/giri).

