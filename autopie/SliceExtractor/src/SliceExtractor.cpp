#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include <filesystem>
#include <fstream>

#include "../../Common/include/Helper.h"
#include "../../Common/include/Streams.h"
#include "../include/Actions.h"

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

using namespace clang;
using namespace llvm;

/**
 * Extracts a slice from source code based on a given list of lines.
 *
 * Call:\n
 * > SliceExtractor.exe [line with error] [slice file] [output (without an extension)] <source path> --
 * e.g. SliceExtractor.exe --loc-line=17 --slice-file="slice.txt" -o="result" example.cpp --
 */
int main(int argc, const char** argv)
{
	// Parse the command-line args passed to the tool.
	tooling::CommonOptionsParser op(argc, argv, AutoPieArgs);

	if (op.getSourcePathList().size() > 1)
	{
		errs() << "Only a single source file is supported.\n";
		return EXIT_FAILURE;
	}

	tooling::ClangTool tool(op.getCompilations(), op.getSourcePathList()[0]);

	// Include paths are not always recognized, especially for standard/system includes.
	// This Adjuster helps with that.
	auto includes = tooling::getInsertArgumentAdjuster("-I/usr/local/lib/clang/11.0.0/include/");
	tool.appendArgumentsAdjuster(includes);

	auto inputLanguage = Language::Unknown;
	// Run a language check inside a separate scope so that all built ASTs get freed at the end.
	{
		Out::Verb() << "Checking the language...\n";

		std::vector<std::unique_ptr<ASTUnit>> trees;
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

	// Check whether the given line is in the file and pretty print it to the standard output.
	if (!CheckLocationValidity(op.getSourcePathList()[0], LineNumber))
	{
		errs() << "The specified error location is invalid!\nSource path: " << op.getSourcePathList()[0]
			<< ", line: " << LineNumber << " could not be found.\n";
	}

	// Get all lines of the slice.
	std::vector<int> sliceLines;
	std::ifstream ifs(SliceFile);

	int lineNumber;
	while (ifs >> lineNumber)
	{
		sliceLines.push_back(lineNumber);
	}

	auto result = tool.run(SliceExtractor::SliceExtractorFrontendActionFactory(sliceLines).get());

	if (result != 0)
	{
		errs() << "The tool returned a non-standard value: " << result << "\n";
	}

	// Keep the relevant lines only.
	ifs.close();
	ifs.open(op.getSourcePathList()[0]);

	auto const outputFileWithCorrectExtension = RemoveFileExtensions(OutputFile) + LanguageToExtension(inputLanguage);
	std::ofstream ofs(outputFileWithCorrectExtension);

	int adjustedErrorLine = LineNumber;

	// Go over the original lines and keep only those that contain a part of the slice or its necessary parts.
	// For example, keep all includes.
	// The error's line number is adjusted accordingly to the number of removed lines.
	if (ifs && ofs)
	{
		Out::Verb() << "Source code after slice extraction:\n";

		std::string line;

		for (auto i = 1; std::getline(ifs, line); i++)
		{
			if (std::find(sliceLines.begin(), sliceLines.end(), i) != sliceLines.end() ||
				std::count_if(line.begin(), line.end(), [](const char c)
				{
					return std::isspace(c);
				}) == line.size() ||
				line.rfind("#include", 0) == 0)
			{
				Out::Verb() << line << "\n";

				ofs << line << "\n";
			}
			else
			{
				if (i <= LineNumber)
				{
					adjustedErrorLine--;
				}
			}
		}
	}
	else
	{
		errs() << "The input or output file could not be opened.\n";
		return EXIT_FAILURE;
	}

	ofs.close();
	ofs.open("adjustedLineNumber.txt");

	if (ofs)
	{
		ofs << adjustedErrorLine;
	}
	else
	{
		errs() << "The input or output file could not be opened.\n";
		return EXIT_FAILURE;
	}

	Out::All() << "Slice extraction done.\n";

	return EXIT_SUCCESS;
}
