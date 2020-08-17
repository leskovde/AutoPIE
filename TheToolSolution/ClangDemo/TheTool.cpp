#include <iostream>
// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"

#include "clang/Driver/Options.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"

using namespace clang;
using namespace driver;
using namespace tooling;
using namespace llvm;


std::string TempName = "tempFile.cpp";

// Global state
Rewriter RewriterInstance;
auto Variants = std::stack<std::string>();

int CurrentLine;
int CountVisitorCurrentLine;

int Iteration = 0;
int NumFunctions = 0;
bool SourceChanged = false;

cl::OptionCategory MyToolCategory("my-tool options");

static SourceRange GetSourceRange(const Stmt& s)
{
	return SourceRange(s.getBeginLoc(), s.getEndLoc());
}

// Traverses the entire AST and reduces it via Visit functions
class ReductionASTVisitor : public RecursiveASTVisitor<ReductionASTVisitor>
{
private:
	ASTContext* ast_context_; // used for getting additional AST info

public:
	explicit ReductionASTVisitor(CompilerInstance* CI)
		: ast_context_(&(CI->getASTContext())) // initialize private members
	{
		RewriterInstance.setSourceMgr(ast_context_->getSourceManager(),
		                              ast_context_->getLangOpts());
	}

	virtual bool VisitFunctionDecl(FunctionDecl* func)
	{
		NumFunctions++;

		const auto funcName = func->getNameInfo().getName().getAsString();

		if (funcName != "main")
		{
			if (func->hasBody())
			{
				const auto body = func->getBody();
				const auto range = SourceRange(body->getBeginLoc(), body->getEndLoc());
				RewriterInstance.ReplaceText(range, ";");
				errs() << "** Removed function body: " << funcName << "\n";
			}
		}

		return true;
	}

	virtual bool VisitStmt(Stmt* st)
	{
		if (const auto ret = dyn_cast<ReturnStmt>(st))
		{
			const auto range = GetSourceRange(*ret);
			RewriterInstance.RemoveText(range);
			errs() << "** Removed ReturnStmt\n";
		}
		if (const auto call = dyn_cast<CallExpr>(st))
		{
			const auto range = GetSourceRange(*call);
			RewriterInstance.RemoveText(range);
			errs() << "** Removed function call\n";
		}

		return true;
	}
};

// Traverses the AST and removes a selected statement set by the CurrentLine number
class LineReductionASTVisitor : public RecursiveASTVisitor<LineReductionASTVisitor>
{
private:
	int iteration_ = 0;
	ASTContext* ast_context_; // used for getting additional AST info

public:
	explicit LineReductionASTVisitor(CompilerInstance* CI)
		: ast_context_(&(CI->getASTContext())) // initialize private members
	{
		SourceChanged = false;
		RewriterInstance = Rewriter();
		RewriterInstance.setSourceMgr(ast_context_->getSourceManager(),
		                              ast_context_->getLangOpts());
	}

	virtual bool VisitStmt(Stmt* st)
	{
		iteration_++;

		if (iteration_ == CurrentLine)
		{
			// Remove non-compound statements first
			if (!st->children().empty())
			{
				CurrentLine++;
				return true;
			}

			const auto range = GetSourceRange(*st);
			RewriterInstance.RemoveText(range);
			SourceChanged = true;

			return false;
		}

		return true;
	}
};

// Counts the number of statements and stores it in CountVisitorCurrentLine
class CountASTVisitor : public RecursiveASTVisitor<CountASTVisitor>
{
private:
	ASTContext* ast_context_; // used for getting additional AST info

public:
	explicit CountASTVisitor(CompilerInstance* CI)
		: ast_context_(&(CI->getASTContext())) // initialize private members
	{
	}

	virtual bool VisitStmt(Stmt* st)
	{
		CountVisitorCurrentLine++;
		return true;
	}
};

class ReductionASTConsumer : public ASTConsumer
{
private:
	ReductionASTVisitor* visitor; // doesn't have to be private

public:
	// override the constructor in order to pass CI
	explicit ReductionASTConsumer(CompilerInstance* CI)
		: visitor(new ReductionASTVisitor(CI)) // initialize the visitor
	{
	}

	// override this to call our ExampleVisitor on the entire source file
	void HandleTranslationUnit(ASTContext& Context) override
	{
		/* we can use ASTContext to get the TranslationUnitDecl, which is
		     a single Decl that collectively represents the entire source file */
		visitor->TraverseDecl(Context.getTranslationUnitDecl());
	}
};

class LineReductionASTConsumer : public ASTConsumer
{
private:
	LineReductionASTVisitor* visitor; // doesn't have to be private

public:
	// override the constructor in order to pass CI
	explicit LineReductionASTConsumer(CompilerInstance* CI)
		: visitor(new LineReductionASTVisitor(CI)) // initialize the visitor
	{
	}

	// override this to call our ExampleVisitor on the entire source file
	void HandleTranslationUnit(ASTContext& Context) override
	{
		/* we can use ASTContext to get the TranslationUnitDecl, which is
			 a single Decl that collectively represents the entire source file */
		visitor->TraverseDecl(Context.getTranslationUnitDecl());
	}
};

class CountASTConsumer : public ASTConsumer
{
private:
	CountASTVisitor* visitor; // doesn't have to be private

public:
	// override the constructor in order to pass CI
	explicit CountASTConsumer(CompilerInstance* CI)
		: visitor(new CountASTVisitor(CI)) // initialize the visitor
	{
		CountVisitorCurrentLine = 0;
	}

	// override this to call our ExampleVisitor on the entire source file
	void HandleTranslationUnit(ASTContext& Context) override
	{
		/* we can use ASTContext to get the TranslationUnitDecl, which is
			 a single Decl that collectively represents the entire source file */
		visitor->TraverseDecl(Context.getTranslationUnitDecl());
	}
};


class ReduceSourceCodeAction : public ASTFrontendAction
{
public:

	// Prints the updated source file to the console and to the TempName file
	void EndSourceFileAction() override
	{
		outs() << "FILE REDUCTION: END OF FILE ACTION:\n";
		auto& SM = RewriterInstance.getSourceMgr();

		RewriterInstance.getEditBuffer(SM.getMainFileID()).write(errs());

		std::error_code error_code;
		raw_fd_ostream outFile(TempName, error_code, sys::fs::F_None);

		RewriterInstance.getEditBuffer(SM.getMainFileID()).write(outFile); // --> this will write the result to outFile
		outFile.close();

		outs() << "\n";
	}

	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& CI, StringRef file) override
	{
		return std::unique_ptr<ASTConsumer>(std::make_unique<ReductionASTConsumer>(&CI));
		// pass CI pointer to ASTConsumer
	}
};

class LineReduceAction : public ASTFrontendAction
{
public:

	// Prints the updated source file to a new file specific to the current iteration
	void EndSourceFileAction() override
	{
		if (!SourceChanged)
			return;

		auto& SM = RewriterInstance.getSourceMgr();
		const auto fileName = "temp/" + std::to_string(Iteration) + TempName;

		std::error_code error_code;
		raw_fd_ostream outFile(fileName, error_code, sys::fs::F_None);

		RewriterInstance.getEditBuffer(SM.getMainFileID()).write(outFile); // --> this will write the result to outFile
		outFile.close();

		Variants.push(fileName);
	}

	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& CI, StringRef file) override
	{
		return std::unique_ptr<ASTConsumer>(std::make_unique<LineReductionASTConsumer>(&CI));
		// pass CI pointer to ASTConsumer
	}
};

class CountAction : public ASTFrontendAction
{
public:

	// Prints the number of statements in the source code to the console
	void EndSourceFileAction() override
	{
		outs() << "COUNT: END OF FILE ACTION:\n";
		outs() << "Statement count: " << CountVisitorCurrentLine;
		outs() << "\n\n";
	}

	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& CI, StringRef file) override
	{
		return std::unique_ptr<ASTConsumer>(std::make_unique<CountASTConsumer>(&CI));
		// pass CI pointer to ASTConsumer
	}
};

// Returns the number statements in the fileName source code file
static int GetStatementCount(CompilationDatabase& CD, const std::string& fileName)
{
	ClangTool tool(CD, fileName);
	auto result = tool.run(newFrontendActionFactory<CountAction>().get());

	return CountVisitorCurrentLine;
}

// Removes lineNumber-th statement in the fileName source code file
static void ReduceStatement(CompilationDatabase& CD, const std::string& fileName, int lineNumber)
{
	CurrentLine = lineNumber;

	ClangTool tool(CD, fileName);
	auto result = tool.run(newFrontendActionFactory<LineReduceAction>().get());
}

extern cl::OptionCategory MyToolCategory;

// Generates all possible variations of the supplied source code
// Call:
// > TheTool.exe [path to a cpp file] --
int main(int argc, const char** argv)
{
	// parse the command-line args passed to the code
	CommonOptionsParser op(argc, argv, MyToolCategory);

	SourceChanged = false;
	Variants.push(*op.getSourcePathList().begin());

	// Search all possible source code variations in a DFS manner
	while (!Variants.empty())
	{
		auto currentFile = Variants.top();
		Variants.pop();

		auto lineCount = GetStatementCount(op.getCompilations(), currentFile);

		// Remove each statement once and push a new file without that statement onto the stack (done inside the FrontEndAction)
		for (auto i = 1; i <= lineCount; i++)
		{
			outs() << "Iteration: " << std::to_string(Iteration) << "\n\n";
			ReduceStatement(op.getCompilations(), currentFile, i);
			Iteration++;
		}
	}

	return 0;
}
