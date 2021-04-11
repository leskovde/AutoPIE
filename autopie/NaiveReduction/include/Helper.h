#ifndef HELPER_H
#define HELPER_H
#pragma once

#include <clang/Basic/SourceLocation.h>
#include <clang/Lex/Lexer.h>
#include <lldb/lldb-enumerations.h>

#include <filesystem>
#include <utility>

/**
 * Path to the temporary directory into which source file variants and executables are generated.\n
 * This path is cleared on each invocation.
 */
inline const char* TempFolder = "../temp/";

/**
 * Path to the GraphViz output directory into which `.dot` files are generated.
 * This path is NOT cleared on each invocation.
 */
inline const char* VisualsFolder = "../visuals/";

class DependencyGraph;

using BitMask = std::vector<bool>;

//===----------------------------------------------------------------------===//
//
/// Input data structures.
//
//===----------------------------------------------------------------------===//

/**
 * Keeps the file name and the line number of the error specified on the command line.
 */
struct Location
{
	std::string fileName;
	int lineNumber;

	Location(std::string locFile, const int locLine) : fileName(std::move(locFile)), lineNumber(locLine)
	{
	}
};

/**
 * Keeps the data specified in the options on the command line.
 */
struct InputData
{
	const std::string errorMessage;
	const Location errorLocation;
	const double reductionRatio;
	const bool dumpDot;

	InputData(std::string message, Location location, const double ratio, const bool dump) : errorMessage(std::move(message)),
	                                                                                         errorLocation(std::move(location)),
	                                                                                         reductionRatio(ratio),
	                                                                                         dumpDot(dump)
	{
	}
};

//===----------------------------------------------------------------------===//
//
/// Source range helper functions.
//
//===----------------------------------------------------------------------===//

std::string RangeToString(clang::ASTContext& astContext, clang::SourceRange range);

clang::SourceRange GetPrintableRange(clang::SourceRange range, const clang::SourceManager& sm);

llvm::StringRef GetSourceTextRaw(clang::SourceRange range, const clang::SourceManager& sm);

llvm::StringRef GetSourceText(clang::SourceRange range, const clang::SourceManager& sm);


//===----------------------------------------------------------------------===//
//
/// File, path, and directory helper functions.
//
//===----------------------------------------------------------------------===//

bool ClearTempDirectory(bool prompt);

std::string RemoveFileExtensions(const std::string& filePath);

std::string EscapeQuotes(const std::string& text);

//===----------------------------------------------------------------------===//
//
/// BitMask helper functions.
//
//===----------------------------------------------------------------------===//

std::string Stringify(BitMask& bitMask);

bool IsFull(BitMask& bitMask);

void Increment(BitMask& bitMask);

bool IsValid(BitMask& bitMask, DependencyGraph& dependencies);

//===----------------------------------------------------------------------===//
//
/// Variant validation helper functions.
//
//===----------------------------------------------------------------------===//

int Compile(const std::filesystem::directory_entry& entry);

std::string StateToString(lldb::StateType state);

std::string StopReasonToString(lldb::StopReason reason);

#endif
