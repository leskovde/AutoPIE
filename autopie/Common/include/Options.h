#ifndef OPTIONS_H
#define OPTIONS_H
#pragma once
#include <llvm/Support/CommandLine.h>
#include <clang/Tooling/CommonOptionsParser.h>

#include "Helper.h"

inline llvm::cl::OptionCategory GeneralArgs("General Options");
inline llvm::cl::OptionCategory DeltaReductionArgs("DeltaReduction Options");
inline llvm::cl::OptionCategory NaiveReductionArgs("NaiveReduction Options");

/**
 * Defines a --help option for the tool. Shows all available options and their descriptions.\n
 * Although the variable is unused, it is still parsed in the CommonOptionsParser.
 */
inline llvm::cl::extrahelp CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);

/**
 * Defines additional text to the CommonHelp. The text is shown when using --help.\n
 * Although the variable is unused, it is still parsed in the CommonOptionsParser.
 */
inline llvm::cl::extrahelp MoreHelp("\nMore help text...");

/**
 * Specifies the source file in which an error was found.\n
 * The value is later used for location confirmation.
 */
inline llvm::cl::opt<std::string> SourceFile("loc-file",
                                             llvm::cl::desc("The name of the file in which the error occured."),
                                             llvm::cl::value_desc("filename"),
                                             llvm::cl::Required,
                                             llvm::cl::cat(GeneralArgs),
                                             llvm::cl::cat(NaiveReductionArgs),
                                             llvm::cl::cat(DeltaReductionArgs));

/**
 * Specifies the line number in the previously specified source file on which an error was found.\n
 * The value is later used for location confirmation.
 */
inline llvm::cl::opt<int> LineNumber("loc-line",
                                     llvm::cl::desc("The line number on which the error occured."),
                                     llvm::cl::value_desc("int"),
                                     llvm::cl::Required,
                                     llvm::cl::cat(GeneralArgs),
                                     llvm::cl::cat(NaiveReductionArgs),
                                     llvm::cl::cat(DeltaReductionArgs));

/**
 * A description of a runtime error. The description should be reproducible, e.g., 'segmentation fault'.\n
 * It is assumed that the description is a substring of the stack trace / exception message seen after launch.\n
 * The value is later used for location confirmation.
 */
inline llvm::cl::opt<std::string> ErrorMessage("error-message",
                                               llvm::cl::desc(
	                                               "A part of the error message specifying the nature of the error."),
                                               llvm::cl::value_desc("string"),
                                               llvm::cl::cat(NaiveReductionArgs),
                                               llvm::cl::cat(DeltaReductionArgs));

/**
 * Specifies the desired best-possible size of the output. Values are between 0 and 1, with 0 being an empty
 * file and 1 being all available variants (including the original) of the source file.\n
 * The value is used to stop the variant search upon hitting a certain threshold.\n
 * E.g., 0.25 => the output file will have (in the worst scenario and if a valid variant is found) roughly a quarter
 * its size in bytes, with three quarters being removed.
 */
inline llvm::cl::opt<double> ReductionRatio("ratio",
                                            llvm::cl::desc("Limits the reduction to a specific ratio between 0 and 1."),
                                            llvm::cl::init(1.0),
                                            llvm::cl::value_desc("double"),
                                            llvm::cl::cat(NaiveReductionArgs));

/**
 * If set to true, the program generates a .dot file containing a graph of code units.\n
 * The file serves to visualize the term 'code units' and also shows dependencies in the source code.\n
 * The value is used after the first few AST passes to check whether an output .dot file should be generated.
 */
inline llvm::cl::opt<bool> DumpDot("dump-dot",
                                   llvm::cl::desc(
	                                   "Specifies whether a GraphViz file containing relationships of code units should be created."),
                                   llvm::cl::init(false),
                                   llvm::cl::value_desc("bool"),
                                   llvm::cl::cat(NaiveReductionArgs),
                                   llvm::cl::cat(DeltaReductionArgs));

inline llvm::cl::alias DumpDotAlias("d",
                                    llvm::cl::desc(
	                                    "Specifies whether a GraphViz file containing relationships of code units should be created."),
                                    llvm::cl::aliasopt(DumpDot));

/**
 * If set to true, the tool provides the user with more detailed information about the process of the reduction.\n
 * Such information contains debug information concerning the parsed AST, the steps taken while processing each
 * variant, the result of the compilation of each variant and its debugging session.
 */
inline llvm::cl::opt<bool> Verbose("verbose",
                                   llvm::cl::desc(
	                                   "Specifies whether the tool should flood the standard output with its optional messages."),
                                   llvm::cl::init(false),
                                   llvm::cl::value_desc("bool"),
                                   llvm::cl::cat(GeneralArgs));

inline llvm::cl::alias VerboseAlias("v",
                                    llvm::cl::desc(
	                                    "Specifies whether the tool should flood the standard output with its optional messages."),
                                    llvm::cl::aliasopt(Verbose));

/**
 * If set to true, the tool directs all of its current output messages into a set path.\n
 * The default path is specified as the variable `LogFile` in the Helper.h file.
 */
inline llvm::cl::opt<bool> LogToFile("log",
                                     llvm::cl::desc(
	                                     "Specifies whether the tool should output its optional message (with timestamps) to an external file. Default path: '"
	                                     + std::string(LogFile) + "'."),
                                     llvm::cl::init(false),
                                     llvm::cl::value_desc("bool"),
                                     llvm::cl::cat(GeneralArgs));

inline llvm::cl::alias LogToFileAlias("l",
                                      llvm::cl::desc(
	                                      "Specifies whether the tool should output its optional message (with timestamps) to an external file. Default path: '"
	                                      + std::string(LogFile) + "'."),
                                      llvm::cl::aliasopt(LogToFile));

#endif
