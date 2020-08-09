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
using namespace clang::driver;
using namespace clang::tooling;
using namespace llvm;

Rewriter rewriter;
int numFunctions = 0;
cl::OptionCategory MyToolCategory("my-tool options");

class ExampleVisitor : public RecursiveASTVisitor<ExampleVisitor>
{
private:
	ASTContext* ast_context_; // used for getting additional AST info

public:
	explicit ExampleVisitor(CompilerInstance* CI)
		: ast_context_(&(CI->getASTContext())) // initialize private members
	{
		rewriter.setSourceMgr(ast_context_->getSourceManager(),
		                      ast_context_->getLangOpts());
	}

	virtual bool VisitFunctionDecl(FunctionDecl* func)
	{
		numFunctions++;
		const auto funcName = func->getNameInfo().getName().getAsString();

		if (funcName != "main")
		{
			rewriter.InsertText(func->getLocation(), "CLANG_MODIFY_");
			errs() << "** Rewrote function def: " << funcName << "\n";

			if (func->hasBody())
			{
				const auto body = func->getBody();
				const auto range = SourceRange(body->getBeginLoc(), body->getEndLoc());
				rewriter.ReplaceText(range, ";");
			}
		}

		return true;
	}

	virtual bool VisitStmt(Stmt* st)
	{
		if (auto ret = dyn_cast<ReturnStmt>(st))
		{
			rewriter.ReplaceText(ret->getRetValue()->getBeginLoc(), 6, "val");
			errs() << "** Rewrote ReturnStmt\n";
		}
		if (const auto call = dyn_cast<CallExpr>(st))
		{
			rewriter.InsertText(call->getBeginLoc(), "CLANG_MODIFY_");
			errs() << "** Rewrote function call\n";
		}
		return true;
	}
};


class ExampleASTConsumer : public ASTConsumer
{
private:
	ExampleVisitor* visitor; // doesn't have to be private

public:
	// override the constructor in order to pass CI
	explicit ExampleASTConsumer(CompilerInstance* CI)
		: visitor(new ExampleVisitor(CI)) // initialize the visitor
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


class ExampleFrontendAction : public ASTFrontendAction
{
public:

	void EndSourceFileAction() override
	{
		outs() << "END OF FILE ACTION:\n";
		rewriter.getEditBuffer(rewriter.getSourceMgr().getMainFileID()).write(errs());
	}

	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance& CI, StringRef file) override
	{
		return std::unique_ptr<ASTConsumer>(std::make_unique<ExampleASTConsumer>(&CI));
		// pass CI pointer to ASTConsumer
	}
};


extern cl::OptionCategory MyToolCategory;

int main(int argc, const char** argv)
{
	// parse the command-line args passed to your code
	CommonOptionsParser op(argc, argv, MyToolCategory);
	// create a new Clang Tool instance (a LibTooling environment)
	ClangTool Tool(op.getCompilations(), op.getSourcePathList());

	// run the Clang Tool, creating a new FrontendAction (explained below)
	const auto result = Tool.run(newFrontendActionFactory<ExampleFrontendAction>().get());

	errs() << "\nFound " << numFunctions << " functions.\n\n";
	// print out the rewritten source code ("rewriter" is a global var.)

	std::cin.get();
	return result;
}
