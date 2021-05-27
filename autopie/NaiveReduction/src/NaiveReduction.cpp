#include <llvm/ADT/ArrayRef.h>
#include <llvm/Object/MachO.h>
#include <llvm/Support/CommandLine.h>

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>

#include <filesystem>

#include "../../Common/include/Context.h"
#include "../../Common/include/Helper.h"
#include "../../Common/include/Options.h"
#include "../../Common/include/Streams.h"
#include "../include/Actions.h"

using namespace llvm;
using namespace Common;

/**
 * Generates a minimal program variant by naively removing statements.
 * 
 * Call:\n
 * > NaiveReduction.exe [line with error] [error description message] [reduction ratio] [runtime arguments] <source path> --
 * e.g. NaiveReduction.exe --loc-line=17 --error-message="segmentation fault" --arguments="arg1 arg2" --ratio=0.5 example.cpp --
 */
int main(int argc, const char** argv)
{
	// Parse the command-line args passed to the tool.
	clang::tooling::CommonOptionsParser op(argc, argv, AutoPieArgs);

	if (op.getSourcePathList().size() > 1)
	{
		errs() << "Only a single source file is supported.\n";
		return EXIT_FAILURE;
	}

	auto parsedInput = InputData(static_cast<std::basic_string<char>>(ErrorMessage),
	                             Location(op.getSourcePathList()[0], LineNumber), ReductionRatio,
	                             DumpDot);

	// Prompt the user to clear the temp directory.
	if (!ClearTempDirectory())
	{
		errs() << "Terminating...\n";
		return EXIT_FAILURE;
	}

	// The epoch count must be specified in the code, since it must have the const qualifier.
	const auto epochCount = 5;

	auto context = GlobalContext(parsedInput, *op.getSourcePathList().begin(), epochCount);
	clang::tooling::ClangTool tool(op.getCompilations(), context.parsedInput.errorLocation.filePath);

	// Include paths are not always recognized, especially for standard/system includes.
	// This Adjuster helps with that.
	auto includes = clang::tooling::getInsertArgumentAdjuster("-I/usr/local/lib/clang/11.0.0/include/");
	tool.appendArgumentsAdjuster(includes);

	auto inputLanguage = clang::Language::Unknown;
	// Run a language check inside a separate scope so that all built ASTs get freed at the end.
	{
		Out::Verb() << "Checking the language...\n";

		std::vector<std::unique_ptr<clang::ASTUnit>> trees;
		tool.buildASTs(trees);

		if (trees.size() != 1)
		{
			errs() << "Although only one AST was expected, " << trees.size() << " ASTs have been built.\n";
			return EXIT_FAILURE;
		}

		inputLanguage = (*trees.begin())->getInputKind().getLanguage();

		Out::Verb() << "File: " << (*trees.begin())->getOriginalSourceFileName() << ", language: " <<
			LanguageToString(inputLanguage) << "\n";
	}

	assert(inputLanguage != clang::Language::Unknown);

	context.language = inputLanguage;

	// Check whether the given line is in the file and pretty print it to the standard output.
	if (!CheckLocationValidity(parsedInput.errorLocation.filePath, parsedInput.errorLocation.lineNumber))
	{
		errs() << "The specified error location is invalid!\nSource path: " << parsedInput.errorLocation.filePath
			<< ", line: " << parsedInput.errorLocation.lineNumber << " could not be found.\n";
	}

	LLDBSentry sentry;

	// Run all Clang AST related actions.
	auto result = tool.run(Naive::VariantGeneratingFrontendActionFactory(context).get());

	if (result != 0)
	{
		errs() << "The tool returned a non-standard value: " << result << "\n";
	}

	return context.stats.exitCode;
}
