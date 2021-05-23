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

#include "../../Common/include/Helper.h"
#include "../../Common/include/Streams.h"

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

using namespace clang;
using namespace ast_matchers;
using namespace llvm;

class DeclRefHandler final : public MatchFinder::MatchCallback
{
public:
	std::vector<std::string> declRefNames;

	DeclRefHandler()
	{
	}

	void run(const MatchFinder::MatchResult& result) override
	{
		if (const auto expr = result.Nodes.getNodeAs<DeclRefExpr>("declRefs"))
		{	
			//expr->dump();

			if (expr->getDecl()->isFunctionOrFunctionTemplate())
			{
				return;
			}
			
			const auto range = GetPrintableRange(GetPrintableRange(expr->getSourceRange(), *result.SourceManager),
				*result.SourceManager);

			for (auto i = result.SourceManager->getSpellingLineNumber(range.getBegin()); 
				i <= result.SourceManager->getSpellingLineNumber(range.getEnd()); i++)
			{
				if (i == LineNumber)
				{
					declRefNames.push_back(expr->getNameInfo().getAsString());
				}
			}
		}
	}
};

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
	tooling::CommonOptionsParser op(argc, argv, AutoPieArgs);

	if (op.getSourcePathList().size() > 1)
	{
		errs() << "Only a single source file is supported.\n";
		return EXIT_FAILURE;
	}

	tooling::ClangTool tool(op.getCompilations(), op.getSourcePathList()[0]);

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

	if (!CheckLocationValidity(op.getSourcePathList()[0], LineNumber))
	{
		errs() << "The specified error location is invalid!\nSource path: " << op.getSourcePathList()[0]
			<< ", line: " << LineNumber << " could not be found.\n";
	}

	DeclRefHandler varHandler;
	MatchFinder finder;
	finder.addMatcher(declRefExpr(isExpansionInMainFile()).bind("declRefs"), &varHandler);

	out::Verb() << "Matching variables...\n";
	
	auto result = tool.run(tooling::newFrontendActionFactory(&finder).get());

	if (result)
	{
		errs() << "The tool returned a non-standard value: " << result << "\n";
	}

	out::Verb() << "Matching done.\n";

	// Remove duplicates.
	std::sort(varHandler.declRefNames.begin(), varHandler.declRefNames.end());
	const auto it = std::unique(varHandler.declRefNames.begin(), varHandler.declRefNames.end());
	varHandler.declRefNames.erase(it, varHandler.declRefNames.end());
	
	std::ofstream ofs(OutputFile);
	
	if (ofs)
	{
		out::Verb() << "List of found variables:\n";

		for (auto& var : varHandler.declRefNames)
		{
			out::Verb() << LineNumber << ":" << var << "\n";
			
			ofs << LineNumber << ":" << var << "\n";
		}
	}
	else
	{
		errs() << "The output file could not be opened.\n";
		return EXIT_FAILURE;
	}

	out::All() << "Variable extraction done.\n";

	return EXIT_SUCCESS;
}