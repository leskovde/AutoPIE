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

static cl::opt<bool> DumpDot("dump-dot",
	cl::desc("(Specifies whether a GraphViz file containing relationships of code units should be created.)"),
	cl::init(false),
	cl::cat(Args));

bool Validate(const char* const userInputError, const std::filesystem::directory_entry& entry)
{
	outs() << "ENTRY: " << entry.path().string() << "\n";
	const std::string compileCommand = "g++ -o temp\\" + entry.path().filename().replace_extension(".exe").string() + " " +
		entry.path().string() + " 2>&1";

	const auto compilationResult = ExecCommand(compileCommand.c_str());
	outs() << "COMPILATION: " << compilationResult << "\n";

	if (compilationResult.find("error") == std::string::npos)
	{
		const std::string debugCommand = "gdb --batch -ex run temp\\" + entry
		                                                          .path().filename().replace_extension(".exe").string()
			+ " 2>&1";

		const auto debugResult = ExecCommand(debugCommand.c_str());
		outs() << "DEBUG: " << debugResult << "\n";

		if (debugResult.find(userInputError) != std::string::npos)
		{
			copy_file(entry, "result.cpp");
			return true;
		}
	}
	return false;
}

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

	auto parsedInput = InputData(ErrorMessage, Location(SourceFile, LineNumber), ReductionRatio, DumpDot);

	// Prompt the user to clear the temp directory.
	if (!ClearTempDirectory(false))
	{
		outs() << "Terminating...\n";
		return 0;
	}

	auto context = GlobalContext(parsedInput, *op.getSourcePathList().begin());

	clang::tooling::ClangTool tool(op.getCompilations(), context.parsedInput.errorLocation.fileName);
	auto result = tool.run(CustomFrontendActionFactory(context).get());

	return 0;
	
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
