#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include <filesystem>
#include <fstream>

#include "../../Common/include/Helper.h"
#include "../../Common/include/Streams.h"

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

using namespace clang;
using namespace ast_matchers;
using namespace llvm;

/**
 * A class for processing and storing data of a DeclRef ASTMatcher.\n
 * The matcher's results - all declaration references - are processed and only variables are extracted.\n
 * The names of those variables are kept in a public container.
 */
class DeclRefHandler final : public MatchFinder::MatchCallback
{
public:
	std::vector<std::string> declRefNames;

	DeclRefHandler()
	= default;

	void run(const MatchFinder::MatchResult& result) override
	{
		if (const auto expr = result.Nodes.getNodeAs<DeclRefExpr>("declRefs"))
		{
			if (expr->getDecl()->isFunctionOrFunctionTemplate())
			{
				return;
			}

			const auto range = GetPrintableRange(GetPrintableRange(expr->getSourceRange(), *result.SourceManager),
			                                     *result.SourceManager);

			// If somehow the reference stretches across multiple lines, handle them accordingly.
			for (auto i = result.SourceManager->getSpellingLineNumber(range.getBegin());
			     i <= result.SourceManager->getSpellingLineNumber(range.getEnd()); i++)
			{
				if (i == LineNumber)
				{
					// Save the name of the variable - the instance.
					declRefNames.push_back(expr->getNameInfo().getAsString());
				}
			}
		}
	}
};

/**
 * Extracts variables on a given line in a given file.
 *
 * Call:\n
 * > VariableExtractor.exe [line with error] [output path] <source path> --
 * e.g. VariableExtractor.exe --loc-line=17 -o="variables.txt" example.cpp --
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

	DeclRefHandler varHandler;
	MatchFinder finder;

	// This ASTMatchers expression matches all declaration references in the given file.
	finder.addMatcher(declRefExpr(isExpansionInMainFile()).bind("declRefs"), &varHandler);

	Out::Verb() << "Matching variables...\n";

	auto result = tool.run(tooling::newFrontendActionFactory(&finder).get());

	if (result != 0)
	{
		errs() << "The tool returned a non-standard value: " << result << "\n";
	}

	Out::Verb() << "Matching done.\n";

	// Remove duplicates.
	std::sort(varHandler.declRefNames.begin(), varHandler.declRefNames.end());
	const auto it = std::unique(varHandler.declRefNames.begin(), varHandler.declRefNames.end());
	varHandler.declRefNames.erase(it, varHandler.declRefNames.end());

	std::ofstream ofs(OutputFile);

	if (ofs)
	{
		Out::Verb() << "List of found variables:\n";

		for (auto& var : varHandler.declRefNames)
		{
			Out::Verb() << LineNumber << ":" << var << "\n";

			ofs << LineNumber << ":" << var << "\n";
		}
	}
	else
	{
		errs() << "The output file could not be opened.\n";
		return EXIT_FAILURE;
	}

	Out::All() << "Variable extraction done.\n";

	return EXIT_SUCCESS;
}
