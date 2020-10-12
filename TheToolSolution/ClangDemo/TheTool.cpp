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

int ErrorLineNumber = 2;

int Iteration = 0;
int NumFunctions = 0;
bool SourceChanged = false;

cl::OptionCategory MyToolCategory("my-tool options");

static SourceRange GetSourceRange(const Stmt& s)
{
	return {s.getBeginLoc(), s.getEndLoc()};
}

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

// Traverses the AST and removes a selected statement set by the CurrentLine number
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
		RewriterInstance = Rewriter();
		RewriterInstance.setSourceMgr(astContext->getSourceManager(),
		                              astContext->getLangOpts());
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

// Counts the number of expressions and stores it in CountVisitorCurrentLine
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

class ExpressionReductionASTConsumer final : public ASTConsumer
{
	StatementReductionASTVisitor* visitor; // doesn't have to be private

public:
	// override the constructor in order to pass CI
	explicit ExpressionReductionASTConsumer(CompilerInstance* ci)
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

class ExpressionReduceAction final : public ASTFrontendAction
{
public:

	// Prints the updated source file to a new file specific to the current iteration
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

		Variants.push(fileName);
	}

	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& ci, StringRef file) override
	{
		return std::unique_ptr<ASTConsumer>(std::make_unique<ExpressionReductionASTConsumer>(&ci));
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

// Returns the number statements in the fileName source code file
static int GetStatementCount(CompilationDatabase& cd, const std::string& fileName)
{
	ClangTool tool(cd, fileName);
	auto result = tool.run(newFrontendActionFactory<CountAction>().get());

	return CountVisitorCurrentLine;
}

// Removes lineNumber-th statement in the fileName source code file
static void ReduceStatement(CompilationDatabase& cd, const std::string& fileName, const int expressionNumber)
{
	CurrentStatementNumber = expressionNumber;

	ClangTool tool(cd, fileName);
	auto result = tool.run(newFrontendActionFactory<ExpressionReduceAction>().get());
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

// Generates all possible variations of the supplied source code
// Call:
// > TheTool.exe [path to a cpp file] --
int main(int argc, const char** argv)
{
	const auto userInputError = "std::invalid_argument";
	// parse the command-line args passed to the code
	CommonOptionsParser op(argc, argv, MyToolCategory);

	// clean the temp directory
	std::filesystem::remove_all("temp/");
	std::filesystem::create_directory("temp");

#if SMALLFILE
	const std::string fileName = R"(C:\Users\Denis\llvm\llvm-project\TestSourceException.cpp)";
#else
	// TOOL CREATION & COMPILATION OVERHEAD DEMO
	const std::string fileName = R"(C:\Users\Denis\source\repos\Diacritics\Diacritics\Diacritics.cpp)";
#endif

	const auto originalExpressionCount = GetStatementCount(op.getCompilations(), *op.getSourcePathList().begin());
	const auto acceptableRatio = 0.85;

	SourceChanged = false;
	Variants.push(*op.getSourcePathList().begin());

	// Search all possible source code variations in a DFS manner
	while (!Variants.empty())
	{
		auto currentFile = Variants.top();
		Variants.pop();

		const auto expressionCount = GetStatementCount(op.getCompilations(), currentFile);

		if ((static_cast<double>(expressionCount) / originalExpressionCount) < acceptableRatio)
		{
			continue;
		}

		// Remove each statement once and push a new file without that statement onto the stack (done inside the FrontEndAction)
		for (auto i = 1; i <= expressionCount; i++)
		{
			outs() << "Iteration: " << std::to_string(Iteration) << "\n\n";
			ReduceStatement(op.getCompilations(), currentFile, i);
			Iteration++;
		}
	}

	outs() << "\n============================================================\n";
	outs() << "VERIFICATION\n";
	
	for (const auto& entry : std::filesystem::directory_iterator("temp/"))
	{
		if (Validate(userInputError, entry)) break;
	}

	outs() << "\n============================================================\n";

	return 0;
}
