#ifndef OPTIONS_H
#define OPTIONS_H
#pragma once
#include <llvm/ADT/ArrayRef.h>
#include <llvm/Support/CommandLine.h>
#include <clang/Tooling/CommonOptionsParser.h>

#include "Helper.h"

using namespace llvm;

cl::OptionCategory Args("AutoPIE Options");

/**
 * Defines a --help option for the tool. Shows all available options and their descriptions.\n
 * Although the variable is unused, it is still parsed in the CommonOptionsParser.
 */
inline cl::extrahelp CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);

/**
 * Defines additional text to the CommonHelp. The text is shown when using --help.\n
 * Although the variable is unused, it is still parsed in the CommonOptionsParser.
 */
inline cl::extrahelp MoreHelp("\nMore help text...");

/**
 * Specifies the source file in which an error was found.\n
 * The value is later used for location confirmation.
 */
inline cl::opt<std::string> SourceFile("loc-file",
    cl::desc("The name of the file in which the error occured."),
    cl::init(""),
    cl::cat(Args));

/**
 * Specifies the line number in the previously specified source file on which an error was found.\n
 * The value is later used for location confirmation.
 */
inline cl::opt<int> LineNumber("loc-line",
    cl::desc("The line number on which the error occured."),
    cl::init(0),
    cl::cat(Args));

/**
 * A description of a runtime error. The description should be reproducible, e.g., 'segmentation fault'.\n
 * It is assumed that the description is a substring of the stack trace / exception message seen after launch.\n
 * The value is later used for location confirmation.
 */
inline cl::opt<std::string> ErrorMessage("error-message",
    cl::desc("A part of the error message specifying the nature of the error."),
    cl::init(""),
    cl::cat(Args));

// TODO: Implement reduction ratio to cut the variant search.
/**
 * Specifies the desired best-possible size of the output. Values are between 0 and 1, with 0 being the unreduced
 * (original) file and 1 being an attempt to fully reduce the source file.\n
 * The value is used to stop the variant search upon hitting a certain threshold.\n
 * E.g., 0.75 => the output file will have (in the best scenario) roughly a quarter its size in bytes,
 * with three quarters being removed.
 */
inline cl::opt<double> ReductionRatio("ratio",
    cl::desc("Limits the reduction to a specific ratio between 0 and 1."),
    cl::init(1.0),
    cl::cat(Args));

/**
 * If set to true, the program generates a .dot file containing a graph of code units.\n
 * The file serves to visualize the term 'code units' and also shows dependencies in the source code.\n
 * The value is used after the first few AST passes to check whether an output .dot file should be generated.
 */
inline cl::opt<bool> DumpDot("dump-dot",
    cl::desc(
        "Specifies whether a GraphViz file containing relationships of code units should be created."),
    cl::init(false),
    cl::cat(Args));

/**
 * If set to true, the tool provides the user with more detailed information about the process of the reduction.\n
 * Such information contains debug information concerning the parsed AST, the steps taken while processing each
 * variant, the result of the compilation of each variant and its debugging session.
 */
inline cl::opt<bool> Verbose("verbose",
	cl::desc("Specifies whether the tool should flood the standard output with its optional messages."),
	cl::init(false),
	cl::cat(Args));

/**
 * If set to true, the tool directs all of its current output messages into a set path.\n
 * The default path is specified as the variable `LogFile` in the Helper.h file.
 */
inline cl::opt<bool> LogToFile("log",
    cl::desc("Specifies whether the tool should output its optional message (with timestamps) to an external file. Default path: '" + std::string(LogFile) + "'."),
	cl::init(false),
	cl::cat(Args));

#endif