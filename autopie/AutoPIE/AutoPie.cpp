#include <iostream>
#include <filesystem>
// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"

#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include "Context.h"
#include "Helper.h"
#include "Actions.h"

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

using namespace clang;
using namespace driver;
using namespace tooling;
using namespace llvm;

// Global state
Rewriter RewriterInstance;
auto Variants = std::stack<std::string>();

int CurrentStatementNumber;
int CountVisitorCurrentLine;
std::string CurrentProcessedFile;

int ErrorLineNumber = 2;

int Iteration = 0;
int NumFunctions = 0;
bool SourceChanged = false;
bool CreateNewRewriter = true;

cl::OptionCategory MyToolCategory("my-tool options");

extern cl::OptionCategory MyToolCategory;

bool Validate(const char* const userInputError, const std::filesystem::directory_entry& entry)
{
	outs() << "ENTRY: " << entry.path().string() << "\n";
	std::string compileCommand = "g++ -o temp\\" + entry.path().filename().replace_extension(".exe").string() + " " +
		entry.path().string() + " 2>&1";

	auto compilationResult = ExecCommand(compileCommand.c_str());
	outs() << "COMPILATION: " << compilationResult << "\n";

	if (compilationResult.find("error") == std::string::npos)
	{
		std::string debugCommand = "gdb --batch -ex run temp\\" + entry
			.path().filename().replace_extension(".exe").string()
			+ " 2>&1";

		auto debugResult = ExecCommand(debugCommand.c_str());
		outs() << "DEBUG: " << debugResult << "\n";

		if (debugResult.find(userInputError) != std::string::npos)
		{
			copy_file(entry, "result.cpp");
			return true;
		}
	}
	return false;
}

// Returns the number statements in the fileName source code file.
int GetLeafStatementCount(GlobalContext& ctx, clang::tooling::CompilationDatabase& cd)
{
	ctx.countVisitorContext.ResetStatementCount();

	clang::tooling::ClangTool tool(cd, ctx.currentFileName);
	auto result = tool.run(clang::tooling::newFrontendActionFactory<CountAction>().get());

	return ctx.countVisitorContext.GetTotalStatementCount();
}

// Removes statementNumber-th statement in the fileName source code file.
std::string ReduceStatement(GlobalContext& ctx, clang::tooling::CompilationDatabase& cd, const int statementNumber)
{
	ctx.statementReductionContext = StatementReductionContext(statementNumber);

	clang::tooling::ClangTool tool(cd, ctx.currentFileName);
	auto result = tool.run(clang::tooling::newFrontendActionFactory<StatementReduceAction>().get());

	return ctx.lastGeneratedFileName;
}

std::vector<std::string> SplitIntoPartitions(GlobalContext& ctx, CompilationDatabase& cd, const int partitionCount)
{
	std::vector<std::string> filePaths{};
	const auto originalStatementCount = GetLeafStatementCount(ctx, cd);

	for (auto i = 0; i < partitionCount; i++)
	{
		ctx.createNewRewriter = true;

		std::string partitionPath;
		
		// TODO: Reduce in range.
		for (auto j = 1; j <= originalStatementCount / partitionCount; j++)
		{
			partitionPath = ReduceStatement(ctx, cd, i * (originalStatementCount / partitionCount) + j);
			ctx.createNewRewriter = false;
			ctx.iteration++;
		}
		
		filePaths.emplace_back(partitionPath);
	}

	return filePaths;
}

// Generates a minimal program variant using delta debugging.
// Call:
// > TheTool.exe [path to a cpp file] -- [runtime error] [error line location]
// e.g. TheTool.exe C:\Users\User\llvm\llvm-project\TestSource.cpp -- std::invalid_argument 15
int main(int argc, const char** argv)
{
	assert(argc > 2);

	const auto userInputError = argv[argc - 2];
	const auto userInputErrorLocation = std::strtol(argv[argc - 1], nullptr, 10);

	// Parse the command-line args passed to the code.
	CommonOptionsParser op(argc, argv, MyToolCategory);

	if (op.getSourcePathList().size() > 1)
	{
		outs() << "Only a single source file is supported.\n";
		return 0;
	}

	// Clean the temp directory.
	std::filesystem::remove_all("temp/");
	std::filesystem::create_directory("temp");

	auto context = GlobalContext::GetInstance(*op.getSourcePathList().begin(), userInputErrorLocation, userInputError);

	auto originalStatementCount = GetLeafStatementCount(*context, op.getCompilations());

	auto partitionCount = 2;
	
	// While able to test & reduce.
	while (partitionCount <= originalStatementCount)
	{
		// Split the current file into partitions (i.e. a set of new files, each of which is a continuous subset of the original).
		auto fileNames = SplitIntoPartitions(*context, op.getCompilations(), partitionCount);

		auto noErrorThrown = true;
		std::string mostRecentFailureInducingFile;

		// Attempt to find a file in which the desired error is thrown.
		for (auto& file : fileNames)
		{
			// If the desired error is thrown, save the path to the currently iterated one (which is also the most recent failure inducing file).
			if (Validate(userInputError, std::filesystem::directory_entry(file)))
			{
				// TODO: Possible recursive call if this was made into a function, the recursive call would guarantee that no possible minimization paths are excluded.
				mostRecentFailureInducingFile = file;
				noErrorThrown = false;
				break;
			}
		}

		// If no errors were thrown, increase the granularity and keep the original file.
		if (noErrorThrown)
		{
			partitionCount *= 2;
		}
		// If the desired error has been encountered, change the original file to the most recent failure inducing (a subset of the original) and start over.
		else
		{
			partitionCount = 2;
			context->currentFileName = mostRecentFailureInducingFile;
			originalStatementCount = GetLeafStatementCount(*context, op.getCompilations());
		}
	}

	outs() << "\n============================================================\n";

	if (Validate(userInputError, std::filesystem::directory_entry(context->currentFileName)))
	{
		outs() << "Minimal program variant: ./temp/" << context->currentFileName << "\n";
	}
	else
	{
		outs() << "Minimal program variant could not be found!\n";
	}

	outs() << "\n============================================================\n";

	return 0;
}
