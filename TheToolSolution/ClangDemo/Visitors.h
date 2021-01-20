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
	std::vector<clang::Stmt*> ignoredNodes;
	clang::ASTContext* astContext; // used for getting additional AST info
	GlobalContext* globalContext;

	std::vector<clang::Stmt*> GetAllChildren(clang::Stmt* st)
	{
		auto retval = std::vector<clang::Stmt*>();

		if (st == nullptr)
		{
			return retval;
		}
		
		for (auto it = st->child_begin(); it != st->child_end(); ++it)
		{
			auto child = *it;
			retval.push_back(child);
			auto recursiveChildren = GetAllChildren(child);
			retval.insert(retval.end(), recursiveChildren.begin(), recursiveChildren.end());
		}

		return retval;
	}
	
	void InvalidateChildren(clang::Stmt* st)
	{
		auto allChildren = GetAllChildren(st);
		ignoredNodes.insert(ignoredNodes.end(), allChildren.begin(), allChildren.end());
	}
	
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

	/**
	 * Gets the portion of the code that corresponds to given SourceRange, including the
	 * last token. Returns expanded macros.
	 *
	 * @see get_source_text_raw()
	 */
	llvm::StringRef get_source_text(clang::SourceRange range, const clang::SourceManager& sm) {
		clang::LangOptions lo;

		// NOTE: sm.getSpellingLoc() used in case the range corresponds to a macro/preprocessed source.
		auto start_loc = sm.getSpellingLoc(range.getBegin());
		auto last_token_loc = sm.getSpellingLoc(range.getEnd());
		auto end_loc = clang::Lexer::getLocForEndOfToken(last_token_loc, 0, sm, lo);
		auto printable_range = clang::SourceRange{ start_loc, end_loc };
		return get_source_text_raw(printable_range, sm);
	}

	/**
	 * Gets the portion of the code that corresponds to given SourceRange exactly as
	 * the range is given.
	 *
	 * @warning The end location of the SourceRange returned by some Clang functions
	 * (such as clang::Expr::getSourceRange) might actually point to the first character
	 * (the "location") of the last token of the expression, rather than the character
	 * past-the-end of the expression like clang::Lexer::getSourceText expects.
	 * get_source_text_raw() does not take this into account. Use get_source_text()
	 * instead if you want to get the source text including the last token.
	 *
	 * @warning This function does not obtain the source of a macro/preprocessor expansion.
	 * Use get_source_text() for that.
	 */
	llvm::StringRef get_source_text_raw(clang::SourceRange range, const clang::SourceManager& sm) {
		return clang::Lexer::getSourceText(clang::CharSourceRange::getCharRange(range), sm, clang::LangOptions());
	}
	
	virtual bool VisitStmt(clang::Stmt* st)
	{
		// TODO: Possible change - change the the number of children from 0 to something more sensible.

		if (st == nullptr || std::find(ignoredNodes.begin(), ignoredNodes.end(), st) != ignoredNodes.end())
		{
			return true;
		}
		
		auto range = GetSourceRange(*st);

		const auto startLineNumber = astContext->getSourceManager().getSpellingLineNumber(range.getBegin());
		const auto endLineNumber = astContext->getSourceManager().getSpellingLineNumber(range.getEnd());
		
		const auto endTokenLoc = astContext->getSourceManager().getSpellingLoc(range.getEnd());

		const auto startLoc = astContext->getSourceManager().getSpellingLoc(range.getBegin());
		const auto endLoc = clang::Lexer::getLocForEndOfToken(endTokenLoc, 0, astContext->getSourceManager(), clang::LangOptions());

		range = clang::SourceRange(startLoc, endLoc);

		if (
			(std::find(globalContext->lines.begin(), globalContext->lines.end(), startLineNumber) != globalContext->lines.end())
			//|| (std::find(globalContext->lines.begin(), globalContext->lines.end(), endLineNumber) != globalContext->lines.end())
			)
		{

			clang::Rewriter localRewriter;
			localRewriter.setSourceMgr(astContext->getSourceManager(), astContext->getLangOpts());

			auto s = get_source_text(range, astContext->getSourceManager());
			
			llvm::outs() << "============================================================\n";
			llvm::outs() << "CODE TO BE REMOVED: " << localRewriter.getRewrittenText(range) << "\n";
			llvm::outs() << range.printToString(astContext->getSourceManager()) << "\n";
			llvm::outs() << "============================================================\n\n";

			globalContext->globalRewriter.RemoveText(range);
			globalContext->statementReductionContext.ChangeSourceStatus();

			InvalidateChildren(st);
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
			/*
			clang::Rewriter localRewriter;
			localRewriter.setSourceMgr(astContext->getSourceManager(), astContext->getLangOpts());

			llvm::outs() << "============================================================\n";
			llvm::outs() << "LEAF STATEMENT:\n";
			llvm::outs() << "CODE:\n" << localRewriter.getRewrittenText(range) << "\n";
			llvm::outs() << range.printToString(astContext->getSourceManager()) << "\n";
			llvm::outs() << "============================================================\n\n";
			*/
		}
		else
		{
			// The statement is an inner node.
			/*
			const auto range = GetSourceRange(*st);
			const auto locStart = astContext->getSourceManager().getPresumedLoc(range.getBegin());
			const auto locEnd = astContext->getSourceManager().getPresumedLoc(range.getEnd());

			clang::Rewriter localRewriter;
			localRewriter.setSourceMgr(astContext->getSourceManager(), astContext->getLangOpts());

			llvm::outs() << "============================================================\n";
			llvm::outs() << "NON-LEAF STATEMENT:\n";
			llvm::outs() << "CODE:\n" << localRewriter.getRewrittenText(range) << "\n";
			llvm::outs() << range.printToString(astContext->getSourceManager()) << "\n";
			llvm::outs() << "============================================================\n\n";
			*/
		}

		return true;
	}
};