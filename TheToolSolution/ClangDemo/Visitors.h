#pragma once
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Rewrite/Core/Rewriter.h>

#include "Helper.h"
#include "Context.h"

// Traverses the AST and removes a selected statement set by the CurrentLine number.
class StatementReductionASTVisitor : public clang::RecursiveASTVisitor<StatementReductionASTVisitor>
{
	int iteration = 0;
	clang::ASTContext* astContext; // used for getting additional AST info
	GlobalContext* globalContext;

public:
	virtual ~StatementReductionASTVisitor() = default;

	StatementReductionASTVisitor(clang::CompilerInstance* ci, GlobalContext* ctx)
		: astContext(&ci->getASTContext()), globalContext(ctx)
	{
		globalContext->globalRewriter.setSourceMgr(astContext->getSourceManager(),
			astContext->getLangOpts());

		if (globalContext->createNewRewriter)
		{
			globalContext->globalRewriter = clang::Rewriter();
		}

		globalContext->globalRewriter.setSourceMgr(astContext->getSourceManager(),
			astContext->getLangOpts());
	}

	virtual bool VisitStmt(clang::Stmt* st)
	{
		// TODO: Possible change - change the the number of children from 0 to something more sensible.

		if (!st->children().empty())
		{
			// Non-leaf statement, continue.
			return true;
		}
		
		iteration++;

		if (iteration == globalContext->statementReductionContext.GetTargetStatementNumber())
		{
			const auto numberOfChildren = GetChildrenCount(st);

			const auto range = GetSourceRange(*st);
			const auto locStart = astContext->getSourceManager().getPresumedLoc(range.getBegin());
			const auto locEnd = astContext->getSourceManager().getPresumedLoc(range.getEnd());

			clang::Rewriter localRewriter;
			localRewriter.setSourceMgr(astContext->getSourceManager(), astContext->getLangOpts());

			llvm::outs() << "\t\t============================================================\n";
			llvm::outs() << "\t\tSTATEMENT NO. " << iteration << "\n";
			llvm::outs() << "CODE: " << localRewriter.getRewrittenText(range) << "\n";
			llvm::outs() << range.printToString(astContext->getSourceManager()) << "\n";
			llvm::outs() << "\t\tCHILDREN COUNT: " << numberOfChildren << "\n";
			llvm::outs() << "\t\t============================================================\n\n";

			//globalContext->statementReductionContext = StatementReductionContext(globalContext->statementReductionContext.GetTargetStatementNumber() + numberOfChildren);

			if (locStart.getLine() != globalContext->errorLineNumber && locEnd.getLine() != globalContext->errorLineNumber)
			{
				globalContext->globalRewriter.RemoveText(range);
				globalContext->statementReductionContext.ChangeSourceStatus();
			}


			return false;
		}

		return true;
	}
};

// Counts the number of leaf statements and stores it in CountVisitorCurrentLine.
class CountASTVisitor : public clang::RecursiveASTVisitor<CountASTVisitor>
{
	clang::ASTContext* astContext; // used for getting additional AST info
	GlobalContext* globalContext;

public:
	CountASTVisitor(clang::CompilerInstance* ci, GlobalContext* ctx)
		: astContext(&ci->getASTContext()), globalContext(ctx)
	{
		globalContext->countVisitorContext = CountVisitorContext();
	}

	virtual bool VisitStmt(clang::Stmt* st)
	{
		// TODO: Possible change - change the the number of children from 0 to something more sensible.
		
		//st->viewAST();
		
		if (st->children().empty())
		{
			// The statement is a leaf statement.
			globalContext->countVisitorContext.Increment();

			const auto range = GetSourceRange(*st);
			const auto locStart = astContext->getSourceManager().getPresumedLoc(range.getBegin());
			const auto locEnd = astContext->getSourceManager().getPresumedLoc(range.getEnd());

			clang::Rewriter localRewriter;
			localRewriter.setSourceMgr(astContext->getSourceManager(), astContext->getLangOpts());

			llvm::outs() << "\t\t============================================================\n";
			llvm::outs() << "\t\tLEAF STATEMENT:\n";
			llvm::outs() << "\t\tCODE:\n" << localRewriter.getRewrittenText(range) << "\n";
			llvm::outs() << range.printToString(astContext->getSourceManager()) << "\n";
			llvm::outs() << "\t\t============================================================\n\n";
		}
		else
		{
			// The statement is an inner node.

			const auto range = GetSourceRange(*st);
			const auto locStart = astContext->getSourceManager().getPresumedLoc(range.getBegin());
			const auto locEnd = astContext->getSourceManager().getPresumedLoc(range.getEnd());

			clang::Rewriter localRewriter;
			localRewriter.setSourceMgr(astContext->getSourceManager(), astContext->getLangOpts());

			llvm::outs() << "\t\t============================================================\n";
			llvm::outs() << "\t\tNON-LEAF STATEMENT:\n";
			llvm::outs() << "\t\tCODE:\n" << localRewriter.getRewrittenText(range) << "\n";
			llvm::outs() << range.printToString(astContext->getSourceManager()) << "\n";
			llvm::outs() << "\t\t============================================================\n\n";
		}
		

		return true;
	}
};