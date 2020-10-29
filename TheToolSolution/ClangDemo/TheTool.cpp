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

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

#define SMALLFILE 1

using namespace clang;
using namespace driver;
using namespace tooling;
using namespace llvm;

std::string TempName = "tempFile.cpp";

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

static SourceRange GetSourceRange(const Stmt& s)
{
	return {s.getBeginLoc(), s.getEndLoc()};
}

// Returns recursively the number of children of a given statement.
static int GetChildrenCount(Stmt* st)
{
	int childrenCount = 0;

	for (auto it = st->children().begin(); it != st->children().end(); ++it)
	{
		childrenCount++;
	}

	for (auto child : st->children())
	{
		childrenCount += GetChildrenCount(child);
	}

	return childrenCount;
}

// Executes a given command line in the 'cmd' environment.
std::string ExecCommand(const char* cmd)
{
	std::array<char, 128> buffer;
	std::string result;
	std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);

	if (!pipe)
	{
		throw std::runtime_error("PILE ERROR: _popen() failed!");
	}

	while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
	{
		result += buffer.data();
	}

	return result;
}

// Traverses the AST and removes a selected statement set by the CurrentLine number.
class StatementReductionASTVisitor : public RecursiveASTVisitor<StatementReductionASTVisitor>
{
	int iteration = 0;
	ASTContext* astContext; // used for getting additional AST info

public:
	virtual ~StatementReductionASTVisitor() = default;

	explicit StatementReductionASTVisitor(CompilerInstance* ci)
		: astContext(&ci->getASTContext()) // initialize private members
	{
		SourceChanged = false;

		if (CreateNewRewriter)
		{
			RewriterInstance = Rewriter();
		}

		RewriterInstance.setSourceMgr(astContext->getSourceManager(),
		                              astContext->getLangOpts());
	}

	virtual bool TraverseDecl(Decl* d)
	{
		return astContext->getSourceManager().isInMainFile(d->getLocation());
	}

	virtual bool VisitStmt(Stmt* st)
	{
		iteration++;

		if (iteration == CurrentStatementNumber)
		{
			auto numberOfChildren = GetChildrenCount(st);

			const auto range = GetSourceRange(*st);
			auto locStart = astContext->getSourceManager().getPresumedLoc(range.getBegin());
			auto locEnd = astContext->getSourceManager().getPresumedLoc(range.getEnd());

			Rewriter rewriter;
			rewriter.setSourceMgr(astContext->getSourceManager(), astContext->getLangOpts());

			outs() << "\n============================================================\n";
			outs() << "STATEMENT NO. " << iteration << "\n";
			outs() << "CODE: " << rewriter.getRewrittenText(range) << "\n";
			outs() << range.printToString(astContext->getSourceManager()) << "\n";
			outs() << "CHILDREN COUNT: " << numberOfChildren << "\n";
			outs() << "\n============================================================\n";

			if (locStart.getLine() != ErrorLineNumber && locEnd.getLine() != ErrorLineNumber)
			{
				RewriterInstance.RemoveText(range);
				SourceChanged = true;
			}

			CurrentStatementNumber += numberOfChildren;

			return false;
		}

		return true;
	}
};

// Counts the number of expressions and stores it in CountVisitorCurrentLine.
class CountASTVisitor : public RecursiveASTVisitor<CountASTVisitor>
{
	ASTContext* astContext; // used for getting additional AST info

public:
	explicit CountASTVisitor(CompilerInstance* ci)
		: astContext(&ci->getASTContext()) // initialize private members
	{
	}

	virtual bool VisitStmt(Stmt* st)
	{
		CountVisitorCurrentLine++;

		return true;
	}
};

class StatementReductionASTConsumer final : public ASTConsumer
{
	StatementReductionASTVisitor* visitor; // doesn't have to be private

public:
	// override the constructor in order to pass CI
	explicit StatementReductionASTConsumer(CompilerInstance* ci)
		: visitor(new StatementReductionASTVisitor(ci)) // initialize the visitor
	{
	}

	// override this to call our ExampleVisitor on the entire source file
	void HandleTranslationUnit(ASTContext& context) override
	{
		/* we can use ASTContext to get the TranslationUnitDecl, which is
			 a single Decl that collectively represents the entire source file */
		visitor->TraverseDecl(context.getTranslationUnitDecl());
	}
};

class CountASTConsumer final : public ASTConsumer
{
	CountASTVisitor* visitor; // doesn't have to be private

public:
	// override the constructor in order to pass CI
	explicit CountASTConsumer(CompilerInstance* ci)
		: visitor(new CountASTVisitor(ci)) // initialize the visitor
	{
		CountVisitorCurrentLine = 0;
	}

	// override this to call our ExampleVisitor on the entire source file
	void HandleTranslationUnit(ASTContext& context) override
	{
		/* we can use ASTContext to get the TranslationUnitDecl, which is
			 a single Decl that collectively represents the entire source file */
		visitor->TraverseDecl(context.getTranslationUnitDecl());
	}
};

class StatementReduceAction final : public ASTFrontendAction
{
public:

	// Prints the updated source file to a new file specific to the current iteration.
	void EndSourceFileAction() override
	{
		if (!SourceChanged)
			return;

		auto& sm = RewriterInstance.getSourceMgr();

		outs() << "EXPR REDUCTION: END OF FILE ACTION:\n";
		RewriterInstance.getEditBuffer(sm.getMainFileID()).write(errs());

		const auto fileName = "temp/" + std::to_string(Iteration) + TempName;

		std::error_code errorCode;
		raw_fd_ostream outFile(fileName, errorCode, sys::fs::F_None);

		RewriterInstance.getEditBuffer(sm.getMainFileID()).write(outFile); // --> this will write the result to outFile
		outFile.close();

		CurrentProcessedFile = fileName;
	}

	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& ci, StringRef file) override
	{
		return std::unique_ptr<ASTConsumer>(std::make_unique<StatementReductionASTConsumer>(&ci));
		// pass CI pointer to ASTConsumer
	}
};

class CountAction final : public ASTFrontendAction
{
public:

	// Prints the number of statements in the source code to the console
	void EndSourceFileAction() override
	{
		outs() << "COUNT: END OF FILE ACTION:\n";
		outs() << "Statement count: " << CountVisitorCurrentLine;
		outs() << "\n\n";
	}

	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& ci, StringRef file) override
	{
		return std::unique_ptr<ASTConsumer>(std::make_unique<CountASTConsumer>(&ci));
		// pass CI pointer to ASTConsumer
	}
};

// Returns the number statements in the fileName source code file.
static int GetStatementCount(CompilationDatabase& cd, const std::string& filePath)
{
	ClangTool tool(cd, filePath);
	auto result = tool.run(newFrontendActionFactory<CountAction>().get());

	return CountVisitorCurrentLine;
}

// Removes lineNumber-th statement in the fileName source code file and returns path to its new file.
static std::string ReduceStatement(CompilationDatabase& cd, const std::string& filePath, const int expressionNumber)
{
	CurrentStatementNumber = expressionNumber;

	ClangTool tool(cd, filePath);
	auto result = tool.run(newFrontendActionFactory<StatementReduceAction>().get());

	return CurrentProcessedFile;
}

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

std::vector<std::string> SplitIntoPartitions(CompilationDatabase& cd, const std::string& filePath,
                                             const int partitionCount)
{
	std::vector<std::string> filePaths{};
	const auto originalStatementCount = GetStatementCount(cd, filePath);

	for (auto i = 0; i < partitionCount; i++)
	{
		CreateNewRewriter = true;

		// TODO: Reduce in range.
		for (auto j = 0; j < originalStatementCount / partitionCount; j++)
		{
			auto partitionPath = ReduceStatement(cd, filePath, i * (originalStatementCount / partitionCount) + j);
			filePaths.emplace_back(partitionPath);
			CreateNewRewriter = false;
			Iteration++;
		}
	}

	return filePaths;
}

// Generates a minimal program variant using delta debugging.
// Call:
// > TheTool.exe [path to a cpp file] -- [runtime error]
// e.g. TheTool.exe C:\Users\User\llvm\llvm-project\TestSource.cpp -- std::invalid_argument
int main(int argc, const char** argv)
{	
	const auto userInputError = argv[argc - 1];

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

	auto originalStatementCount = GetStatementCount(op.getCompilations(), *op.getSourcePathList().begin());

	auto partitionCount = 2;
	auto currentFile = *op.getSourcePathList().begin();

	// While able to test & reduce.
	while (partitionCount <= originalStatementCount)
	{
		// Split the current file into partitions (i.e. a set of new files, each of which is a continuous subset of the original).
		auto fileNames = SplitIntoPartitions(op.getCompilations(), currentFile, partitionCount);

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
			currentFile = mostRecentFailureInducingFile;
			originalStatementCount = GetStatementCount(op.getCompilations(), *op.getSourcePathList().begin());
		}
	}

	outs() << "\n============================================================\n";

	if (Validate(userInputError, std::filesystem::directory_entry(currentFile)))
	{
		outs() << "Minimal program variant: ./temp/" << currentFile << "\n";
	}
	else
	{
		outs() << "Minimal program variant could not be found!\n";
	}

	outs() << "\n============================================================\n";

	return 0;
}
