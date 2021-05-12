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

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

using namespace clang;
using namespace ast_matchers;
using namespace llvm;

cl::OptionCategory VarExtractorArgs("my-tool options");

/**
 * Specifies the source file in which an error was found.\n
 * The value is later used for location confirmation.
 */
inline cl::opt<std::string> SourceFile("loc-file",
	cl::desc("The name of the file in which the error occured."),
	cl::init(""),
	cl::cat(VarExtractorArgs));

/**
 * Specifies the line number in the previously specified source file on which an error was found.\n
 * The value is later used for location confirmation.
 */
inline cl::opt<int> LineNumber("loc-line",
	cl::desc("The line number on which the error occured."),
	cl::init(0),
	cl::cat(VarExtractorArgs));

/**
 * If set to true, the tool provides the user with more detailed information about the process of the reduction.\n
 * Such information contains debug information concerning the parsed AST, the steps taken while processing each
 * variant, the result of the compilation of each variant and its debugging session.
 */
inline cl::opt<bool> Verbose("verbose",
	cl::desc("Specifies whether the tool should flood the standard output with its optional messages."),
	cl::init(false),
	cl::cat(VarExtractorArgs));

/**
 * If set to true, the tool directs all of its current output messages into a set path.\n
 * The default path is specified as the variable `LogFile` in the Helper.h file.
 */
inline cl::opt<bool> LogToFile("log",
	cl::desc("Specifies whether the tool should output its optional message (with timestamps) to an external file. Default path: '" + std::string(LogFile) + "'."),
	cl::init(false),
	cl::cat(VarExtractorArgs));

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
 * Generates a minimal program variant by naively removing statements.
 *
 * Call:\n
 * > NaiveReduction.exe [file with error] [line with error] [error description message] [reduction ratio] <source path 0> [... <source path N>] --
 * e.g. NaiveReduction.exe --loc-file="example.cpp" --loc-line=17 --error-message="segmentation fault" --ratio=0.5 example.cpp --
 */
int main(int argc, const char** argv)
{
	// Parse the command-line args passed to the tool.
	tooling::CommonOptionsParser op(argc, argv, VarExtractorArgs);

	// TODO(Denis): Create multi-file support.
	if (op.getSourcePathList().size() > 1)
	{
		errs() << "Only a single source file is supported.\n";
		return EXIT_FAILURE;
	}

	auto parsedInput = InputData(static_cast<std::basic_string<char>>(ErrorMessage),
		Location(static_cast<std::basic_string<char>>(SourceFile), LineNumber), ReductionRatio,
		DumpDot);


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