#include <iostream>
#include <filesystem>

#include "llvm/Support/CommandLine.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
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

using namespace llvm;


static cl::OptionCategory Args("AutoPIE Options");
static cl::extrahelp CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp("\nMore help text...");

static cl::opt<std::string> SourceFile("loc-file",
	cl::desc("(The name of the file in which the error occured.)"),
	cl::init(""),
	cl::cat(Args));

static cl::opt<int> LineNumber("loc-line",
	cl::desc("(The line number on which the error occured.)"),
	cl::init(0),
	cl::cat(Args));

static cl::opt<std::string> ErrorMessage("error-message",
	cl::desc("(A part of the error message specifying the nature of the error.)"),
	cl::init(""),
	cl::cat(Args));

static cl::opt<double> ReductionRatio("ratio",
	cl::desc("(Limits the reduction to a specific ratio between 0 and 1.)"),
	cl::init(1.0),
	cl::cat(Args));

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
	auto result = tool.run(clang::tooling::newFrontendActionFactory<VariantGenerationAction>().get());

	ctx.iteration++;
}

/*
unsigned long GenerateVariants(GlobalContext* context, clang::tooling::CommonOptionsParser& op, const int originalSourceSize)
{
	unsigned long variantCount = 0;
	
	if (context->searchStack.empty())
	{
		outs() << "DEBUG: Global context contains 0 variants - the input file has not been queued.\n";
	}
	
	while (!context->searchStack.empty())
	{
		auto currentFile = context->searchStack.top();
		context->searchStack.pop();

		if (variantCount % 50 == 0)
		{
			outs() << "Done " << variantCount << " variants. Current stack size: " << context->searchStack.size() << "\n";
		}
		
		variantCount++;

		const auto variantSize = GetStatementCount(*context, op.getCompilations(), currentFile);

		if (static_cast<double>(variantSize) / originalSourceSize < context->parsedInput.reductionRatio)
		{
			continue;
		}

		// TODO: Start from here.

		// Remove each statement once and push a new file without that statement onto the stack (done inside the FrontEndAction).
		for (auto i = 1; i <= variantSize; i++)
		{
			outs() << "\tIteration: " << std::to_string(context->iteration) << " (targeting statement no. " << i << " )\n\n";
			ReduceStatement(*context, op.getCompilations(), currentFile, i);
		}
	}

	return variantCount;
}
*/

/**
 * Generates a minimal program variant by naively removing statements.
 * 
 * Call:
 * > AutoPIE.exe [file with error] [line with error] [error description message] [reduction ratio] <source path 0> [... <source path N>] --
 * e.g. AutoPIE.exe --loc-file="example.cpp" --loc-line=17 --error-message="stack overflow" --ratio=0.5 example.cpp --
 */
int main(int argc, const char** argv)
{
	// Parse the command-line args passed to the tool.
	clang::tooling::CommonOptionsParser op(argc, argv, Args);

	// TODO: Create multi-file support.
	if (op.getSourcePathList().size() > 1)
	{
		outs() << "Only a single source file is supported.\n";
		return 0;
	}

	auto parsedInput = InputData(ErrorMessage, Location(SourceFile, LineNumber), ReductionRatio);

	// Prompt the user to clear the temp directory.
	if (!ClearTempDirectory(true))
	{
		llvm::outs() << "Terminating...\n";
		return 0;
	}

	auto context = GlobalContext(parsedInput, *op.getSourcePathList().begin());

	clang::tooling::ClangTool tool(op.getCompilations(), context.parsedInput.errorLocation.fileName);
	auto result = tool.run(CustomFrontendActionFactory(context).get());
	
	// TODO: Prevent duplicate program variants during generation => both search and verification speedup.

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
		if (Validate(ErrorMessage.c_str(), entry))
		{
			outs() << "Minimal program variant: ./temp/" << entry.path().c_str() << "\n";
			minimalProgramVariantFound = true;
			break;
		}
	}

	if (!minimalProgramVariantFound)
	{
		outs() << "Minimal program variant could not be found.\n";
	}

	outs() << "\n============================================================\n";

	return 0;
}
