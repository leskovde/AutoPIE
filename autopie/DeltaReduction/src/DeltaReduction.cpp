#include <llvm/ADT/ArrayRef.h>
#include <llvm/Object/MachO.h>
#include <llvm/Support/CommandLine.h>

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>

#include <filesystem>

#include "../include/Actions.h"
#include "../../Common/include/Context.h"
#include "../../Common/include/Helper.h"
#include "../../Common/include/Options.h"
#include "../../Common/include/Streams.h"

using namespace llvm;
using namespace Common;

// TODO: Change all int instances to something consistent (int32_fast etc.).

/**
 * Generates a minimal program variant by naively removing statements.
 * 
 * Call:\n
 * > NaiveReduction.exe [file with error] [line with error] [error description message] [reduction ratio] <source path 0> [... <source path N>] --
 * e.g. NaiveReduction.exe --loc-file="example.cpp" --loc-line=17 --error-message="segmentation fault" --ratio=0.5 example.cpp --
 */
int main(int argc, const char** argv)
{
	// Parse the command-line args passed to the tool.
	clang::tooling::CommonOptionsParser op(argc, argv, Args);

	// TODO(Denis): Create multi-file support.
	if (op.getSourcePathList().size() > 1)
	{
		errs() << "Only a single source file is supported.\n";
		return EXIT_FAILURE;
	}
	
	auto parsedInput = InputData(static_cast<std::basic_string<char>>(ErrorMessage),
	                             Location(static_cast<std::basic_string<char>>(SourceFile), LineNumber), ReductionRatio,
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

	auto includes = clang::tooling::getInsertArgumentAdjuster("-I/usr/local/lib/clang/11.0.0/include/");
	tool.appendArgumentsAdjuster(includes);
	
	auto inputLanguage = clang::Language::Unknown;
	// Run a language check inside a separate scope so that all built ASTs get freed at the end.
	{
		out::Verb() << "Checking the language...\n";
		
		std::vector<std::unique_ptr<clang::ASTUnit>> trees;
		tool.buildASTs(trees);

		if (trees.size() != 1)
		{
			errs() << "Although only one AST was expected, " << trees.size() << " ASTs have been built.\n";
			return EXIT_FAILURE;
		}
		
		inputLanguage = (*trees.begin())->getInputKind().getLanguage();
		
		out::Verb() << "File: " << (*trees.begin())->getOriginalSourceFileName() << ", language: " << LanguageToString(inputLanguage) << "\n";
	}

	assert(inputLanguage != clang::Language::Unknown);

	context.language = inputLanguage;

	if (!CheckLocationValidity(parsedInput.errorLocation.filePath, parsedInput.errorLocation.lineNumber))
	{
		errs() << "The specified error location is invalid!\nSource path: " << parsedInput.errorLocation.filePath
			<< ", line: " << parsedInput.errorLocation.lineNumber << " could not be found.\n";
	}

	LLDBSentry sentry;


	auto iteration = 0;
	const auto cutOffLimit = 0xffff;

	auto partitionCount = 2;
	auto currentTestCase = context.parsedInput.errorLocation.filePath;

	auto done = false;
	
	while (!done && iteration < cutOffLimit)
	{
		iteration++;

		if (iteration % 10 == 0)
		{
			out::All() << "Done " << iteration << " DD iterations.\n";
		}

		DeltaIterationResults iterationResult;

		clang::tooling::ClangTool newTool(op.getCompilations(), currentTestCase);
		newTool.appendArgumentsAdjuster(includes);

		// Run all Clang AST related actions.
		auto result = newTool.run(Delta::DeltaDebuggingFrontendActionFactory(context, iteration, partitionCount, iterationResult).get());

		if (result)
		{
			errs() << "The tool returned a non-standard value: " << result << "\n";
		}

		switch (iterationResult)
		{
		case DeltaIterationResults::FailingPartition:
			partitionCount = 2;
			currentTestCase = TempFolder + std::to_string(iteration) + "_" + GetFileName(context.parsedInput.errorLocation.filePath) + LanguageToExtension(context.language);
			break;
		case DeltaIterationResults::FailingComplement:
			partitionCount -= 1;
			currentTestCase = TempFolder + std::to_string(iteration) + "_" + GetFileName(context.parsedInput.errorLocation.filePath) + LanguageToExtension(context.language);
			break;
		case DeltaIterationResults::Passing:
			partitionCount *= 2;			
			break;
		case DeltaIterationResults::Unsplitable:
			done = true;
		default:
			throw std::invalid_argument("Invalid iteration result.");
		}
	}

	out::All() << "Finished. Done " << iteration << " DD iterations.\n";

	if (currentTestCase != context.parsedInput.errorLocation.filePath)
	{
		const auto newFileName = TempFolder + std::string("autoPieOut") + LanguageToExtension(context.language);

		out::All() << "Found the locally minimal error-inducing source file: " << currentTestCase << "\n";
		out::All() << "Changing the file path to '" << newFileName << "'.\n";

		std::filesystem::rename(currentTestCase, newFileName);

		return EXIT_SUCCESS;
	}

	out::All() << "A smaller error-inducing source file could not be found.\n";
	
	return EXIT_FAILURE;
}
