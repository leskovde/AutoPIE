#pragma once
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Rewrite/Core/Rewriter.h>

#include "Helper.h"
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
	
public:
	VariantPrintingASTVisitor(clang::CompilerInstance* ci, GlobalContext& ctx)
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

	void SetOutputFile(const std::string& fileName)
	{
		outputFile = fileName;
	}

	void SetBitMask(const BitMask& mask)
	{
		bitMask = mask;
	}
};

class MappingASTVisitor : public clang::RecursiveASTVisitor<MappingASTVisitor>
{
	clang::ASTContext& astContext_;
	GlobalContext& globalContext_;
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

	MappingASTVisitor(clang::CompilerInstance* ci, GlobalContext& ctx, const NodeMappingRef mapping)
		: astContext_(ci->getASTContext()), globalContext_(ctx), nodeMapping_(mapping)
	{}

	virtual bool VisitDecl(clang::Decl* decl)
	{
		InsertMapping(decl->getID(), codeUnitsCount);

		codeUnitsCount++;
		return true;
	}

	virtual bool VisitExpr(clang::Expr* expr)
	{
		InsertMapping(expr->getID(astContext_), codeUnitsCount);

		codeUnitsCount++;
		return true;
	}

	virtual bool VisitStmt(clang::Stmt* stmt)
	{
		InsertMapping(stmt->getID(astContext_), codeUnitsCount);

		codeUnitsCount++;
		return true;
	}
};

class DependencyASTVisitor : public clang::RecursiveASTVisitor<DependencyASTVisitor>
{
	clang::ASTContext& astContext_;
	GlobalContext& globalContext_;
	NodeMappingRef nodeMapping_;
	
public:
	int codeUnitsCount = 0;
	DependencyGraph graph = DependencyGraph();
	
	DependencyASTVisitor(clang::CompilerInstance* ci, GlobalContext& ctx, const NodeMappingRef mapping)
		: astContext_(ci->getASTContext()), globalContext_(ctx), nodeMapping_(mapping)
	{}

	virtual bool VisitExpr(clang::Expr* expr)
	{
		return true;
	}
	
	virtual bool VisitStmt(clang::Stmt* stmt)
	{
		for (auto& child : stmt->children())
		{
			graph.InsertDependency((*nodeMapping_)[stmt->getID(astContext_)], (*nodeMapping_)[child->getID(astContext_)]);
		}
		
		return true;
	}
};