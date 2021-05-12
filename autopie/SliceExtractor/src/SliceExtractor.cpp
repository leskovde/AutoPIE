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

#include "../include/Context.h"
#include "../include/Actions.h"
#include "../../Common/include/Helper.h"
#include "../../Common/include/Streams.h"

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

using namespace clang;
using namespace ast_matchers;
using namespace llvm;

class FuncDeclHandler final : public MatchFinder::MatchCallback
{
public:
	std::tuple<int, int> lineRange;

	explicit FuncDeclHandler()
	{
	}

	void run(const MatchFinder::MatchResult& result) override
	{
		if (const auto decl = result.Nodes.getNodeAs<FunctionDecl>("mainFunction"))
		{
			//decl->dump();

			const auto bodyRange = GetPrintableRange(GetPrintableRange(decl->getSourceRange(), *result.SourceManager),
				*result.SourceManager);
			
			const auto startLineNumber = result.Context->getSourceManager().getSpellingLineNumber(bodyRange.getBegin());
			const auto endLineNumber = result.Context->getSourceManager().getSpellingLineNumber(bodyRange.getEnd());
			
			lineRange = std::make_tuple(startLineNumber, endLineNumber);
			
			outs() << "Range of the main function: [" << startLineNumber << " - " << endLineNumber << "]\n";
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

	if (!CheckLocationValidity(SourceFile, LineNumber))
	{
		errs() << "The specified error location is invalid!\nSource path: " << SourceFile
			<< ", line: " << LineNumber << " could not be found.\n";
	}

	FuncDeclHandler handlerForMain;
	MatchFinder finder;
	finder.addMatcher(functionDecl(hasName("main")).bind("mainFunction"), &handlerForMain);

	auto result = tool.run(tooling::newFrontendActionFactory(&finder).get());

	if (result)
	{
		errs() << "The tool returned a non-standard value: " << result << "\n";
	}

	// Get all lines of the slice.
	std::ifstream ifs(SliceFile);

	
	
	// Keep only the relevant lines.
	ifs.close();
	ifs.open(SourceFile);
	std::ofstream ofs(OutputFile);

	if (ifs && ofs)
	{
		out::Verb() << "Source code after slice extraction:\n";

		std::string line;
		
		for (auto i = 1; std::getline(ifs, line); i++)
		{
			out::Verb() << line;

			ofs << line;
		}
	}
	else
	{
		errs() << "The input or output file could not be opened.\n";
		return EXIT_FAILURE;
	}

	out::All() << "Variable extraction done.\n";

	return EXIT_SUCCESS;
}