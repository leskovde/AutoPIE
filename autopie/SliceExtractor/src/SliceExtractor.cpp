#include <iostream>
#include <filesystem>
#include <fstream>
// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

#include "../include/Actions.h"
#include "../../Common/include/Helper.h"
#include "../../Common/include/Streams.h"

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

using namespace clang;
using namespace llvm;

/**
 * Extract variables on a given line in a given file.
 *
 * Call:\n
 * > VariableExtractor.exe [file with error] [line with error] <source path> --
 * e.g. VariableExtractor.exe --loc-file="example.cpp" --loc-line=17 example.cpp --
 */
int main(int argc, const char** argv)
{
	// Parse the command-line args passed to the tool.
	tooling::CommonOptionsParser op(argc, argv, GeneralArgs);

	if (op.getSourcePathList().size() > 1)
	{
		errs() << "Only a single source file is supported.\n";
		return EXIT_FAILURE;
	}

	tooling::ClangTool tool(op.getCompilations(), SourceFile);

	auto includes = tooling::getInsertArgumentAdjuster("-I/usr/local/lib/clang/11.0.0/include/");
	tool.appendArgumentsAdjuster(includes);

	auto inputLanguage = Language::Unknown;
	// Run a language check inside a separate scope so that all built ASTs get freed at the end.
	{
		out::Verb() << "Checking the language...\n";

		std::vector<std::unique_ptr<ASTUnit>> trees;
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

	// Get all lines of the slice.
	std::vector<int> sliceLines;
	std::ifstream ifs(SliceFile);

	int lineNumber;
	while (ifs >> lineNumber)
	{
		sliceLines.push_back(lineNumber);
	}

	auto result = tool.run(SliceExtractor::SliceExtractorFrontendActionFactory(sliceLines).get());

	if (result)
	{
		errs() << "The tool returned a non-standard value: " << result << "\n";
	}
	
	// Keep the relevant lines only.
	ifs.close();
	ifs.open(SourceFile);

	auto const outputFileWithCorrectExtension = RemoveFileExtensions(OutputFile) + LanguageToExtension(inputLanguage);
	std::ofstream ofs(outputFileWithCorrectExtension);

	int adjustedErrorLine = LineNumber;
	
	if (ifs && ofs)
	{
		out::Verb() << "Source code after slice extraction:\n";

		std::string line;
		
		for (auto i = 1; std::getline(ifs, line); i++)
		{
			if (std::find(sliceLines.begin(), sliceLines.end(), i) != sliceLines.end())
			{
				out::Verb() << line << "\n";

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

	out::All() << "Variable extraction done.\n";

	return EXIT_SUCCESS;
}