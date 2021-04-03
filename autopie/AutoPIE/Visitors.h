#pragma once
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Rewrite/Core/Rewriter.h>

#include "Helper.h"
#include "Context.h"
#include "DependencyGraph.h"

class VariantPrintingASTVisitor;
class MappingASTVisitor;
class DependencyASTVisitor;

using NodeMapping = std::unordered_map<int, int>;
using NodeMappingRef = std::shared_ptr<std::unordered_map<int, int>>;
using VariantPrintingASTVisitorRef = std::unique_ptr<VariantPrintingASTVisitor>;
using MappingASTVisitorRef = std::unique_ptr<MappingASTVisitor>;
using DependencyASTVisitorRef = std::unique_ptr<DependencyASTVisitor>;

// Traverses the AST and removes a selected statement set by the CurrentLine number.
class VariantPrintingASTVisitor : public clang::RecursiveASTVisitor<VariantPrintingASTVisitor>
{
	clang::ASTContext& astContext;
	GlobalContext& globalContext;

	BitMask bitMask;
	int currentNode = 0;
	std::string outputFile;

	void Print(const clang::SourceRange range) const
	{
		/*
		const auto startLineNumber = astContext.getSourceManager().getSpellingLineNumber(range.getBegin());
		const auto endLineNumber = astContext.getSourceManager().getSpellingLineNumber(range.getEnd());

		const auto endTokenLoc = astContext.getSourceManager().getSpellingLoc(range.getEnd());

		const auto startLoc = astContext.getSourceManager().getSpellingLoc(range.getBegin());
		const auto endLoc = clang::Lexer::getLocForEndOfToken(endTokenLoc, 0, astContext.getSourceManager(), clang::LangOptions());

		range = clang::SourceRange(startLoc, endLoc);

		clang::Rewriter localRewriter;
		localRewriter.setSourceMgr(astContext.getSourceManager(), astContext.getLangOpts());

		auto s = GetSourceText(range, astContext.getSourceManager());
		*/
		
		llvm::outs() << RangeToString(astContext, range);
		
		//llvm::outs() << localRewriter.getRewrittenText(range) << "\n";
		//llvm::outs() << range.printToString(astContext.getSourceManager()) << "\n";

		//globalContext->globalRewriter.RemoveText(range);
	}
	
public:
	VariantPrintingASTVisitor(clang::CompilerInstance* ci, GlobalContext& ctx)
		: astContext(ci->getASTContext()), globalContext(ctx)
	{
		globalContext.globalRewriter = clang::Rewriter();
		globalContext.globalRewriter.setSourceMgr(astContext.getSourceManager(),
			astContext.getLangOpts());
	}

	virtual bool VisitDecl(clang::Decl* decl)
	{
		if (bitMask[currentNode])
		{
			const auto range = decl->getSourceRange();

			Print(range);
		}
		
		currentNode++;
		return true;
	}

	virtual bool VisitExpr(clang::Expr* expr)
	{
		if (bitMask[currentNode])
		{
			const auto range = expr->getSourceRange();

			Print(range);
		}
	
		currentNode++;
		return true;
	}
	
	virtual bool VisitStmt(clang::Stmt* stmt)
	{
		if (bitMask[currentNode])
		{
			const auto range = stmt->getSourceRange();

			Print(range);
		}
	
		currentNode++;
		return true;
	}
	
	/*
	virtual bool VisitForStmt(clang::ForStmt* stmt)
	{
		if (bitMask[currentNode])
		{
			const auto range = stmt->getSourceRange();
			Print(range);
		}

		currentNode++;
		return true;
	}
	*/
	
	void Reset(const std::string& fileName, const BitMask& mask)
	{
		currentNode = 0;
		bitMask = mask;
		outputFile = fileName;
	}
};

class MappingASTVisitor : public clang::RecursiveASTVisitor<MappingASTVisitor>
{
	clang::ASTContext& astContext_;
	NodeMappingRef nodeMapping_;

	void InsertMapping(int astId, int myId) const
	{		
		auto const success = nodeMapping_->insert(std::pair<int, int>(astId, myId)).second;

		if (!success)
		{
			llvm::outs() << "Could not insert (" << astId << ", " << myId << ") to the map.\n";
		}
	}

public:
	int codeUnitsCount = 0;

	MappingASTVisitor(clang::CompilerInstance* ci, const NodeMappingRef& mapping)
		: astContext_(ci->getASTContext()), nodeMapping_(mapping)
	{}

	bool VisitDecl(clang::Decl* decl)
	{
		// TODO: Create a more sophisticated mapping that filters duplicates.
		// E.g. VarDecl and DeclStmt share the same source code - filter based on source code.
		if (nodeMapping_->find(decl->getID()) == nodeMapping_->end())
		{
			llvm::outs() << "Node " << codeUnitsCount << ": Type " << decl->getDeclKindName() << "\n";
			InsertMapping(decl->getID(), codeUnitsCount);
			codeUnitsCount++;
		}

		return true;
	}

	bool VisitCallExpr(clang::CallExpr* expr)
	{
		if (nodeMapping_->find(expr->getID(astContext_)) == nodeMapping_->end())
		{
			InsertMapping(expr->getID(astContext_), codeUnitsCount);
			codeUnitsCount++;
		}
		
		return true;
	}

	bool VisitStmt(clang::Stmt* stmt)
	{
		if (llvm::isa<clang::Expr>(stmt))
		{
			// Ignore expressions in general since they tend to be too small.
			return true;
		}
		
		if (nodeMapping_->find(stmt->getID(astContext_)) == nodeMapping_->end())
		{
			llvm::outs() << "Node " << codeUnitsCount << ": Type " << stmt->getStmtClassName() << "\n";
			InsertMapping(stmt->getID(astContext_), codeUnitsCount);
			codeUnitsCount++;
		}
		
		return true;
	}

	/*
	virtual bool VisitForStmt(clang::ForStmt* stmt)
	{
		if (nodeMapping_->find(stmt->getID(astContext_)) == nodeMapping_->end())
		{
			InsertMapping(stmt->getID(astContext_), codeUnitsCount);
			codeUnitsCount++;
		}

		return true;
	}
	*/
};

class DependencyASTVisitor : public clang::RecursiveASTVisitor<DependencyASTVisitor>
{
	clang::ASTContext& astContext_;
	GlobalContext& globalContext_;
	NodeMappingRef nodeMapping_;
	
public:
	//int codeUnitsCount = 0;
	DependencyGraph graph = DependencyGraph();
	
	DependencyASTVisitor(clang::CompilerInstance* ci, GlobalContext& ctx, const NodeMappingRef& mapping)
		: astContext_(ci->getASTContext()), globalContext_(ctx), nodeMapping_(mapping)
	{}

	virtual bool VisitDecl(clang::Decl* decl)
	{
		graph.InsertCodeSnippetForDebugging(nodeMapping_->at(decl->getID()),
			RangeToString(astContext_, decl->getSourceRange()));

		return true;
	}

	bool VisitCallExpr(clang::CallExpr* expr)
	{
		graph.InsertCodeSnippetForDebugging(nodeMapping_->at(expr->getID(astContext_)),
			RangeToString(astContext_, expr->getSourceRange()));

		return true;
	}
	
	virtual bool VisitStmt(clang::Stmt* stmt)
	{
		if (llvm::isa<clang::Expr>(stmt))
		{
			// Ignore expressions in general since they tend to be too small.
			return true;
		}
		
		graph.InsertCodeSnippetForDebugging(nodeMapping_->at(stmt->getID(astContext_)), 
			RangeToString(astContext_, stmt->getSourceRange()));
		
		// Apparently only statements have children.
		for (auto it = stmt->child_begin(); it != stmt->child_end(); ++it)
		{
			if (*it && nodeMapping_->find(it->getID(astContext_)) != nodeMapping_->end())
			{
				graph.InsertDependency(nodeMapping_->at(stmt->getID(astContext_)),
					nodeMapping_->at(it->getID(astContext_)));
			}
		}
		
		return true;
	}

	/*
	virtual bool VisitForStmt(clang::ForStmt* stmt)
	{
		for (auto it = stmt->child_begin(); it != stmt->child_end(); ++it)
		{
			if (*it)
			{
				graph.InsertDependency((*nodeMapping_)[stmt->getID(astContext_)], (*nodeMapping_)[it->getID(astContext_)]);
			}
		}

		return true;
	}
	*/
};