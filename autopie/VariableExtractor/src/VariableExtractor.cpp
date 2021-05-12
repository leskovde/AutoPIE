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

namespace VarExtractor
{
	cl::OptionCategory Args("VariableExtractor Options");

	/**
	 * Specifies the source file in which an error was found.\n
	 * The value is later used for location confirmation.
	 */
	cl::opt<std::string> SourceFile("loc-file",
		cl::desc("The name of the file in which the error occured."),
		cl::init(""),
		cl::cat(Args));

	/**
	 * Specifies the line number in the previously specified source file on which an error was found.\n
	 * The value is later used for location confirmation.
	 */
	cl::opt<int> LineNumber("loc-line",
		cl::desc("The line number on which the error occured."),
		cl::init(0),
		cl::cat(Args));

	/**
	 * If set to true, the tool provides the user with more detailed information about the process of the reduction.\n
	 * Such information contains debug information concerning the parsed AST, the steps taken while processing each
	 * variant, the result of the compilation of each variant and its debugging session.
	 */
	inline cl::opt<bool> Verbose("verbose",
		cl::desc("Specifies whether the tool should flood the standard output with its optional messages."),
		cl::init(false),
		cl::cat(Args));

	cl::alias VerboseAlias("v",
		cl::desc(
			"Specifies whether the tool should flood the standard output with its optional messages."),
		cl::aliasopt(Verbose));
	
	/**
	 * If set to true, the tool directs all of its current output messages into a set path.\n
	 * The default path is specified as the variable `LogFile` in the Helper.h file.
	 */
	cl::opt<bool> LogToFile("log",
		cl::desc("Specifies whether the tool should output its optional message (with timestamps) to an external file. Default path: '" + std::string(LogFile) + "'."),
		cl::init(false),
		cl::cat(Args));

	cl::alias LogToFileAlias("l",
		cl::desc(
			"Specifies whether the tool should output its optional message (with timestamps) to an external file. Default path: '"
			+ std::string(LogFile) + "'."),
		cl::aliasopt(LogToFile));
}
	
class DeclRefHandler : public MatchFinder::MatchCallback
{
public:
	std::tuple<int, int> lineRange;

	DeclRefHandler()
	{
	}

	void run(const MatchFinder::MatchResult& result) override
	{
		if (const FunctionDecl *FD = result.Nodes.getNodeAs<FunctionDecl>("mainFunction"))
		{
			//FD->dump();

			auto bodyRange = FD->getBody()->getSourceRange();
			
			const auto startLineNumber = result.Context->getSourceManager().getSpellingLineNumber(bodyRange.getBegin());
			const auto endLineNumber = result.Context->getSourceManager().getSpellingLineNumber(bodyRange.getEnd());

			const auto endTokenLoc = result.Context->getSourceManager().getSpellingLoc(bodyRange.getEnd());

			const auto startLoc = result.Context->getSourceManager().getSpellingLoc(bodyRange.getBegin());
			const auto endLoc = Lexer::getLocForEndOfToken(endTokenLoc, 0, result.Context->getSourceManager(), LangOptions());

			bodyRange = SourceRange(startLoc, endLoc);
			lineRange = std::make_tuple(startLineNumber, endLineNumber);
			
			outs() << "Range: " << bodyRange.printToString(result.Context->getSourceManager()) << " (lines " << startLineNumber << " - " << endLineNumber << ")\n";
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
	tooling::CommonOptionsParser op(argc, argv, VarExtractor::Args);

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
	
	std::string line;
	std::ifstream ifs(argv[3]);
	std::vector<int> sliceLines;
	
	while (std::getline(ifs, line))
	{
		sliceLines.push_back(std::stoi(line, nullptr, 10));
	}

	// Clean the temp directory.
	std::filesystem::remove_all("temp/");
	std::filesystem::create_directory("temp");

	auto context = GlobalContext::GetInstance(*op.getSourcePathList().begin());

	return 0;
}