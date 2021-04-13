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

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

using namespace clang;
using namespace ast_matchers;
using namespace driver;
using namespace tooling;
using namespace llvm;

cl::OptionCategory MyToolCategory("my-tool options");

extern cl::OptionCategory MyToolCategory;

// Returns the number statements in the fileName source code file.
int GetLeafStatementCount(GlobalContext& ctx, clang::tooling::CompilationDatabase& cd)
{
	ctx.countVisitorContext.ResetStatementCount();

	clang::tooling::ClangTool tool(cd, ctx.currentFileName);
	auto result = tool.run(clang::tooling::newFrontendActionFactory<CountAction>().get());

	return ctx.countVisitorContext.GetTotalStatementCount();
}

// Removes statementNumber-th statement in the fileName source code file.
std::string ReduceStatement(GlobalContext& ctx, clang::tooling::CompilationDatabase& cd, std::vector<int>& lines)
{
	ctx.lines = lines;

	clang::tooling::ClangTool tool(cd, ctx.currentFileName);
	auto result = tool.run(clang::tooling::newFrontendActionFactory<StatementReduceAction>().get());

	return ctx.lastGeneratedFileName;
}

class FuncDeclHandler : public MatchFinder::MatchCallback
{
public:
	std::tuple<int, int> lineRange;

	explicit FuncDeclHandler() {}

	virtual void run(const MatchFinder::MatchResult &Result)
	{
		if (const FunctionDecl *FD = Result.Nodes.getNodeAs<clang::FunctionDecl>("mainFunction"))
		{
			//FD->dump();

			auto bodyRange = FD->getBody()->getSourceRange();
			
			const auto startLineNumber = Result.Context->getSourceManager().getSpellingLineNumber(bodyRange.getBegin());
			const auto endLineNumber = Result.Context->getSourceManager().getSpellingLineNumber(bodyRange.getEnd());

			const auto endTokenLoc = Result.Context->getSourceManager().getSpellingLoc(bodyRange.getEnd());

			const auto startLoc = Result.Context->getSourceManager().getSpellingLoc(bodyRange.getBegin());
			const auto endLoc = clang::Lexer::getLocForEndOfToken(endTokenLoc, 0, Result.Context->getSourceManager(), clang::LangOptions());

			bodyRange = clang::SourceRange(startLoc, endLoc);
			lineRange = std::make_tuple(startLineNumber, endLineNumber);
			
			outs() << "Range: " << bodyRange.printToString(Result.Context->getSourceManager()) << " (lines " << startLineNumber << " - " << endLineNumber << ")\n";
		}
	}
};

std::vector<int> GetAllLineNumbersOfMain(clang::tooling::CompilationDatabase& cd, const std::string& fileName)
{
	FuncDeclHandler handlerForMain;

	MatchFinder finder;
	finder.addMatcher(functionDecl(hasName("main")).bind("mainFunction"), &handlerForMain);
	
	clang::tooling::ClangTool tool(cd, fileName);
	auto result = tool.run(newFrontendActionFactory(&finder).get());

	if (result)
	{
		throw std::invalid_argument("Could not compile the given file!");
	}
	
	auto lineRange = handlerForMain.lineRange;

	auto retval = std::vector<int>();

	for (auto i = std::get<0>(lineRange) + 1; i < std::get<1>(lineRange); i++)
	{
		retval.push_back(i);
	}

	return retval;
}

std::vector<int> GetLineNumbersComplement(const std::vector<int> allLines, const std::vector<int>& sliceLines)
{
	auto retval = std::vector<int>();

	for (auto line : allLines)
	{
		if (std::find(sliceLines.cbegin(), sliceLines.cend(), line) == sliceLines.cend())
		{
			retval.push_back(line);
		}
	}

	return retval;
}

// Generates a minimal program variant using delta debugging.
// Call:
// > TheTool.exe [path to a cpp file] -- [slice line number sequence]
// e.g. TheTool.exe C:\Users\User\llvm\llvm-project\TestSource.cpp -- std::invalid_argument 15
int main(int argc, const char** argv)
{
	assert(argc > 2);

	// Parse the command-line args passed to the code.
	CommonOptionsParser op(argc, argv, MyToolCategory);

	if (op.getSourcePathList().size() > 1)
	{
		outs() << "Only a single source file is supported.\n";
		return 0;
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

	auto allLines = GetAllLineNumbersOfMain(op.getCompilations(), context->currentFileName);
	auto lines = GetLineNumbersComplement(allLines, sliceLines);
	
	auto newFile = ReduceStatement(*context, op.getCompilations(), lines);

	return 0;
}