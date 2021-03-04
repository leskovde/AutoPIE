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
int GetStatementCount(GlobalContext& ctx, clang::tooling::CompilationDatabase& cd, const std::string& fileName)
{
	ctx.countVisitorContext.ResetStatementCount();

	clang::tooling::ClangTool tool(cd, fileName);
	auto result = tool.run(clang::tooling::newFrontendActionFactory<CountAction>().get());
	
	return ctx.countVisitorContext.GetTotalStatementCount();
}

// Removes statementNumber-th statement in the fileName source code file.
void ReduceStatement(GlobalContext& ctx, clang::tooling::CompilationDatabase& cd, const std::string& fileName, const int statementNumber)
{
	ctx.statementReductionContext = StatementReductionContext(statementNumber);

	clang::tooling::ClangTool tool(cd, fileName);
	auto result = tool.run(clang::tooling::newFrontendActionFactory<StatementReduceAction>().get());

	ctx.iteration++;
}

// Generates a minimal program variant using delta debugging.
// Call:
// > TheTool.exe [path to a cpp file] -- [runtime error] [error line location] [reduction ratio]
// e.g. TheTool.exe C:\Users\User\llvm\llvm-project\TestSource.cpp -- std::invalid_argument
int main(int argc, const char** argv)
{
	assert(argc > 2);
	
	const auto acceptableRatio = std::strtol(argv[argc - 1], nullptr, 10);
	const auto userInputError = argv[argc - 3];
	const auto userInputErrorLocation = std::strtol(argv[argc - 2], nullptr, 10);

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

	const auto originalStatementCount = GetStatementCount(*context, op.getCompilations(), *op.getSourcePathList().begin());

	// TODO: Prevent duplicate program variants during generation => both search and verification speedup.
	
	// Search all possible source code variations in a DFS manner.
	while (!context->variants.empty())
	{
		auto currentFile = context->variants.top();
		context->variants.pop();

		outs() << "REDUCING: " << currentFile << " with " << std::to_string(context->variants.size()) << " files remaining on the stack.\n";

		const auto expressionCount = GetStatementCount(*context, op.getCompilations(), currentFile);

		if ((static_cast<double>(expressionCount) / originalStatementCount) < acceptableRatio)
		{
			continue;
		}

		// Remove each statement once and push a new file without that statement onto the stack (done inside the FrontEndAction).
		for (auto i = 1; i <= expressionCount; i++)
		{
			outs() << "\tIteration: " << std::to_string(context->iteration) << " (targeting statement no. " << i << " )\n\n";
			ReduceStatement(*context, op.getCompilations(), currentFile, i);
		}
	}

	outs() << "\n============================================================\n";
	outs() << "VERIFICATION\n";

	std::vector<std::filesystem::directory_entry> files{};

	for (const auto& entry : std::filesystem::directory_iterator("temp/"))
	{
		files.emplace_back(entry);
	}

	// Sort the output by size and iterate it from the smallest to the largest file. The first valid file is the minimal version.
	std::sort(files.begin(), files.end(),
	          [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry b) -> bool
	          {
		          return a.file_size() < b.file_size();
	          });

	auto minimalProgramVariantFound = false;

	for (const auto& entry : files)
	{
		if (Validate(userInputError, entry))
		{
			outs() << "Minimal program variant: ./temp/" << entry.path().c_str() << "\n";
			minimalProgramVariantFound = true;
			break;
		}
	}

	if (!minimalProgramVariantFound)
	{
		outs() << "Minimal program variant could not be found.";
	}

	outs() << "\n============================================================\n";

	return 0;
}
