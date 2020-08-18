#include <iostream>
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
	return {s.getBeginLoc(), s.getEndLoc()};
}

// Traverses the entire AST and reduces it via Visit functions
class ReductionASTVisitor : public RecursiveASTVisitor<ReductionASTVisitor>
{
	ASTContext* astContext; // used for getting additional AST info

public:
	virtual ~ReductionASTVisitor() = default;

	explicit ReductionASTVisitor(CompilerInstance* ci)
		: astContext(&ci->getASTContext()) // initialize private members
	{
		RewriterInstance.setSourceMgr(astContext->getSourceManager(),
		                              astContext->getLangOpts());
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
#ifdef VARIATIONS
		RewriterInstance = Rewriter();
#endif
		RewriterInstance.setSourceMgr(astContext->getSourceManager(),
		                              astContext->getLangOpts());
	}

	virtual bool VisitStmt(Stmt* st)
	{
		iteration++;

		if (iteration == CurrentLine)
		{
			// Remove non-compound statements first
			if (st->children().empty())
			{
				const auto range = GetSourceRange(*st);
				RewriterInstance.RemoveText(range);
				SourceChanged = true;
			}

			return false;
		}

		return true;
	}
};

// Counts the number of statements and stores it in CountVisitorCurrentLine
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

class ReductionASTConsumer final : public ASTConsumer
{
	ReductionASTVisitor* visitor; // doesn't have to be private

public:
	// override the constructor in order to pass CI
	explicit ReductionASTConsumer(CompilerInstance* ci)
		: visitor(new ReductionASTVisitor(ci)) // initialize the visitor
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


class ReduceSourceCodeAction final : public ASTFrontendAction
{
public:

	// Prints the updated source file to the console and to the TempName file
	void EndSourceFileAction() override
	{
		outs() << "FILE REDUCTION: END OF FILE ACTION:\n";
		auto& sm = RewriterInstance.getSourceMgr();

		RewriterInstance.getEditBuffer(sm.getMainFileID()).write(errs());

		std::error_code errorCode;
		raw_fd_ostream outFile(TempName, errorCode, sys::fs::F_None);

		RewriterInstance.getEditBuffer(sm.getMainFileID()).write(outFile); // --> this will write the result to outFile
		outFile.close();

		outs() << "\n";
	}

	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& ci, StringRef file) override
	{
		return std::unique_ptr<ASTConsumer>(std::make_unique<ReductionASTConsumer>(&ci));
		// pass CI pointer to ASTConsumer
	}
};

class StatementReduceAction final : public ASTFrontendAction
{
public:

	// Prints the updated source file to a new file specific to the current iteration
	void EndSourceFileAction() override
	{
		if (!SourceChanged)
			return;

		auto& sm = RewriterInstance.getSourceMgr();

		outs() << "STMT REDUCTION: END OF FILE ACTION:\n";
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

// Returns the number statements in the fileName source code file
static int GetStatementCount(CompilationDatabase& cd, const std::string& fileName)
{
	ClangTool tool(cd, fileName);
	auto result = tool.run(newFrontendActionFactory<CountAction>().get());

	return CountVisitorCurrentLine;
}

// Removes lineNumber-th statement in the fileName source code file
static void ReduceStatement(CompilationDatabase& cd, const std::string& fileName, const int lineNumber)
{
	CurrentLine = lineNumber;

	ClangTool tool(cd, fileName);
	auto result = tool.run(newFrontendActionFactory<StatementReduceAction>().get());
}

extern cl::OptionCategory MyToolCategory;

// Generates all possible variations of the supplied source code
// Call:
// > TheTool.exe [path to a cpp file] --
int main(int argc, const char** argv)
{
	// parse the command-line args passed to the code
	CommonOptionsParser op(argc, argv, MyToolCategory);

#if VARIATIONS
	SourceChanged = false;
	Variants.push(*op.getSourcePathList().begin());

	// Search all possible source code variations in a DFS manner
	while (!Variants.empty())
	{
		auto currentFile = Variants.top();
		Variants.pop();

		const auto lineCount = GetStatementCount(op.getCompilations(), currentFile);

		// Remove each statement once and push a new file without that statement onto the stack (done inside the FrontEndAction)
		for (auto i = 1; i <= lineCount; i++)
		{
			outs() << "Iteration: " << std::to_string(Iteration) << "\n\n";
			ReduceStatement(op.getCompilations(), currentFile, i);
			Iteration++;
		}
	}
#else
	const auto lineCount = GetStatementCount(op.getCompilations(), *op.getSourcePathList().begin());

	// Remove each statement once and push a new file without that statement onto the stack (done inside the FrontEndAction)
	for (auto i = 1; i <= lineCount; i++)
	{
		outs() << "\n\nIteration: " << std::to_string(Iteration) << "\n\n";
		ReduceStatement(op.getCompilations(), *op.getSourcePathList().begin(), i);
		Iteration++;
	}
#endif
	
	return 0;
}
