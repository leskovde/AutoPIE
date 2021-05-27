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
 * Generates a locally minimal program variant by running Delta debugging.
 * 
 * Call:\n
 * > DeltaReduction.exe [line with error] [error description message] [runtime arguments] <source path> --
 * e.g. DeltaReduction.exe --loc-line=17 --error-message="segmentation fault" --arguments="arg1 arg2" example.cpp --
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

	auto iteration = 0;
	// End the search after a given number of iterations.
	// No point in repeating the mistakes of the naive search...
	const auto cutOffLimit = 0xffff;

	auto partitionCount = 2;
	auto currentTestCase = context.parsedInput.errorLocation.filePath;

	auto done = false;
	auto first = true;

	// Iterate until convergence (or until patience runs out) and call the iteration handler.
	// Collect the results of the iteration and determine the next step.
	while (!done && iteration < cutOffLimit)
	{
		iteration++;

		if (iteration % 20 == 0)
		{
			Out::All() << "Done " << iteration << " DD iterations.\n";
		}

		DeltaIterationResults iterationResult;

		clang::tooling::ClangTool newTool(op.getCompilations(), currentTestCase);
		newTool.appendArgumentsAdjuster(includes);

		// Run all Clang AST related actions.
		auto result = newTool.run(
			Delta::DeltaDebuggingFrontendActionFactory(context, iteration, partitionCount, iterationResult).get());

		if (result != 0)
		{
			errs() << "The tool returned a non-standard value: " << result << "\n";
		}

		// Save some statistics concerning the worst-case running time.
		if (first)
		{
			double k = context.deltaContext.latestCodeUnitCount;
			context.stats.expectedIterations = k * k + 3 * k;
			first = false;
		}

		// Process the iteration result and decide the next step for the algorithm.
		switch (iterationResult)
		{
		case DeltaIterationResults::FailingPartition:
			partitionCount = 2;
			currentTestCase = TempFolder + std::to_string(iteration) + "_" + GetFileName(
				context.parsedInput.errorLocation.filePath) + LanguageToExtension(context.language);
			break;
		case DeltaIterationResults::FailingComplement:
			partitionCount -= 1;
			currentTestCase = TempFolder + std::to_string(iteration) + "_" + GetFileName(
				context.parsedInput.errorLocation.filePath) + LanguageToExtension(context.language);
			break;
		case DeltaIterationResults::Passing:
			if (partitionCount * 2 < context.deltaContext.latestCodeUnitCount || partitionCount == context
			                                                                                       .deltaContext.
			                                                                                       latestCodeUnitCount)
			{
				partitionCount *= 2;
			}
			else
			{
				partitionCount = context.deltaContext.latestCodeUnitCount;
			}
			break;
		case DeltaIterationResults::Unsplitable:
			done = true;
			break;
		default:
			throw std::invalid_argument("Invalid iteration result.");
		}
	}

	Out::All() << "Finished. Done " << iteration << " DD iterations.\n";

	// Save the result no matter the outcome.
	const auto newFileName = TempFolder + std::string("autoPieOut") + LanguageToExtension(context.language);

	Out::All() << "Found the locally minimal error-inducing source file: " << currentTestCase << "\n";
	Out::All() << "Changing the file path to '" << newFileName << "'.\n";

	std::filesystem::rename(currentTestCase, newFileName);

	// Print the results and the statistics of the search no matter the outcome.
	PrintResult(newFileName);

	context.stats.Finalize(newFileName);
	DisplayStats(context.stats);

	// Decide to tell the user whether the outcome was successful or not.
	if (currentTestCase != context.parsedInput.errorLocation.filePath)
	{
		return EXIT_SUCCESS;
	}

	Out::All() << "A smaller error-inducing source file could not be found.\n";

	return EXIT_FAILURE;
}
