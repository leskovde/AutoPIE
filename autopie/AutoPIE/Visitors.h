#pragma once
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Rewrite/Core/Rewriter.h>

#include "Helper.h"

// Traverses the AST and removes a selected statement set by the CurrentLine number.
class StatementReductionASTVisitor : public clang::RecursiveASTVisitor<StatementReductionASTVisitor>
{
	int iteration = 0;
	clang::ASTContext& astContext;
	GlobalContext& globalContext;

public:
	StatementReductionASTVisitor(clang::CompilerInstance* ci, GlobalContext& ctx)
		: astContext(ci->getASTContext()), globalContext(ctx)
	{
		globalContext.globalRewriter = clang::Rewriter();
		globalContext.globalRewriter.setSourceMgr(astContext.getSourceManager(),
			astContext.getLangOpts());
	}

	virtual bool VisitStmt(clang::Stmt* st)
	{
		iteration++;

		if (iteration == globalContext.statementReductionContext.GetTargetStatementNumber())
		{
			const auto numberOfChildren = GetChildrenCount(st);

			const auto range = GetSourceRange(*st);
			const auto locStart = astContext.getSourceManager().getPresumedLoc(range.getBegin());
			const auto locEnd = astContext.getSourceManager().getPresumedLoc(range.getEnd());

			clang::Rewriter localRewriter;
			localRewriter.setSourceMgr(astContext.getSourceManager(), astContext.getLangOpts());

			llvm::outs() << "\t\t============================================================\n";
			llvm::outs() << "\t\tSTATEMENT NO. " << iteration << "\n";
			llvm::outs() << "CODE: " << localRewriter.getRewrittenText(range) << "\n";
			llvm::outs() << range.printToString(astContext.getSourceManager()) << "\n";
			llvm::outs() << "\t\tCHILDREN COUNT: " << numberOfChildren << "\n";
			llvm::outs() << "\t\t============================================================\n\n";

			//globalContext->statementReductionContext = StatementReductionContext(globalContext->statementReductionContext.GetTargetStatementNumber() + numberOfChildren);

			if (locStart.getLine() != globalContext.parsedInput.errorLocation.lineNumber && 
				locEnd.getLine() != globalContext.parsedInput.errorLocation.lineNumber)
			{
				globalContext.globalRewriter.RemoveText(range);
				globalContext.statementReductionContext.ChangeSourceStatus();
			}

			return false;
		}

		return true;
	}
};

// Counts the number of expressions and stores it in CountVisitorCurrentLine.
class CountASTVisitor : public clang::RecursiveASTVisitor<CountASTVisitor>
{
	clang::ASTContext& astContext;
	GlobalContext& globalContext;

public:
	CountASTVisitor(clang::CompilerInstance* ci, GlobalContext& ctx)
		: astContext(ci->getASTContext()), globalContext(ctx)
	{
		globalContext.countVisitorContext = CountVisitorContext();
	}

	virtual bool VisitStmt(clang::Stmt* st)
	{
		globalContext.countVisitorContext.Increment();

		return true;
	}
};

using StatementReductionASTVisitorRef = std::unique_ptr<StatementReductionASTVisitor>;
using CountASTVisitorRef = std::unique_ptr<CountASTVisitor>;