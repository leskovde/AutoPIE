#pragma once
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <utility>

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
using RewriterRef = std::shared_ptr<clang::Rewriter>;
using SkippedMapRef = std::shared_ptr<std::unordered_map<int, bool>>;

// Traverses the AST and removes a selected statement set by the CurrentLine number.
class VariantPrintingASTVisitor : public clang::RecursiveASTVisitor<VariantPrintingASTVisitor>
{
	clang::ASTContext& astContext_;
	GlobalContext& globalContext_;

	BitMask bitMask_;
	int currentNode_ = 0;
	RewriterRef rewriter_;
	SkippedMapRef skippedNodes_;
	
	void RemoveFromSource(const clang::SourceRange range) const
	{		
		//llvm::outs() << RangeToString(astContext_, range);
		
		//llvm::outs() << localRewriter.getRewrittenText(range) << "\n";
		//llvm::outs() << range.printToString(astContext.getSourceManager()) << "\n";

		if (rewriter_)
		{
			rewriter_->RemoveText(GetPrintableRange(range, astContext_.getSourceManager()));
		}
		else
		{
			llvm::outs() << "ERROR: Rewriter not set in the VariantPrintingASTVisitor!\n";
		}
		
	}
	
public:
	VariantPrintingASTVisitor(clang::CompilerInstance* ci, GlobalContext& ctx)
		: astContext_(ci->getASTContext()), globalContext_(ctx)
	{
		globalContext_.globalRewriter = clang::Rewriter();
		globalContext_.globalRewriter.setSourceMgr(astContext_.getSourceManager(),
			astContext_.getLangOpts());
	}

	void Reset(const BitMask& mask, RewriterRef& rewriter)
	{
		currentNode_ = 0;
		bitMask_ = mask;
		rewriter_ = rewriter;
	}

	void SetSkippedNodes(SkippedMapRef skippedNodes)
	{
		skippedNodes_ = std::move(skippedNodes);
	}
	
	bool shouldTraversePostOrder() const
	{
		return true;
	}

	virtual bool VisitDecl(clang::Decl* decl)
	{
		if (skippedNodes_->find(currentNode_) == skippedNodes_->end())
		{
			if (!bitMask_[currentNode_])
			{
				const auto range = decl->getSourceRange();

				RemoveFromSource(range);
			}
			
			currentNode_++;
		}
		
		return true;
	}

	bool VisitCallExpr(clang::CallExpr* expr)
	{
		if (skippedNodes_->find(currentNode_) == skippedNodes_->end())
		{
			if (!bitMask_[currentNode_])
			{
				const auto range = expr->getSourceRange();

				RemoveFromSource(range);
			}

			currentNode_++;
		}
		
		return true;
	}
	
	virtual bool VisitStmt(clang::Stmt* stmt)
	{
		if (skippedNodes_->find(currentNode_) == skippedNodes_->end())
		{
			if (!bitMask_[currentNode_])
			{
				const auto range = stmt->getSourceRange();

				RemoveFromSource(range);
			}

			currentNode_++;
		}

		return true;
	}
};

class MappingASTVisitor : public clang::RecursiveASTVisitor<MappingASTVisitor>
{
	clang::ASTContext& astContext_;
	NodeMappingRef nodeMapping_;
	std::unordered_map<std::string, bool> snippetSet_;
	SkippedMapRef skippedNodes_ = std::make_shared<std::unordered_map<int, bool>>();

	void InsertMapping(int astId, int myId, const std::string& snippet)
	{
		if (snippetSet_.find(snippet) != snippetSet_.end())
		{
			// This node's code has already been processed.
			skippedNodes_->insert(std::pair<int, bool>(codeUnitsCount, true));
			return;
		}
		
		auto const success = nodeMapping_->insert(std::pair<int, int>(astId, myId)).second;

		if (success)
		{
			snippetSet_[snippet] = true;
			graph.InsertCodeSnippetForDebugging(codeUnitsCount, snippet);
			
			codeUnitsCount++;
		}
		else
		{
			skippedNodes_->insert(std::pair<int, bool>(codeUnitsCount, true));
			llvm::outs() << "Could not insert (" << astId << ", " << myId << ") to the map.\n";
		}
	}

public:
	int codeUnitsCount = 0;
	DependencyGraph graph = DependencyGraph();

	MappingASTVisitor(clang::CompilerInstance* ci, const NodeMappingRef& mapping)
		: astContext_(ci->getASTContext()), nodeMapping_(mapping)
	{}

	SkippedMapRef GetSkippedNodes()
	{
		return skippedNodes_;
	}
	
	bool shouldTraversePostOrder() const
	{
		return true;
	}
	
	bool VisitDecl(clang::Decl* decl)
	{
		// TODO: Create a more sophisticated mapping that filters duplicates.
		// E.g. VarDecl and DeclStmt share the same source code - filter based on source code.
		if (nodeMapping_->find(decl->getID()) == nodeMapping_->end())
		{
			llvm::outs() << "Node " << codeUnitsCount << ": Type " << decl->getDeclKindName() << "\n";
			InsertMapping(decl->getID(), codeUnitsCount, RangeToString(astContext_, decl->getSourceRange()));
		}
		
		return true;
	}

	bool VisitCallExpr(clang::CallExpr* expr)
	{
		if (nodeMapping_->find(expr->getID(astContext_)) == nodeMapping_->end())
		{
			InsertMapping(expr->getID(astContext_), codeUnitsCount, RangeToString(astContext_, expr->getSourceRange()));
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
			InsertMapping(stmt->getID(astContext_), codeUnitsCount, RangeToString(astContext_, stmt->getSourceRange()));

			// Apparently only statements have children.
			for (auto it = stmt->child_begin(); it != stmt->child_end(); ++it)
			{
				if (*it && nodeMapping_->find(it->getID(astContext_)) != nodeMapping_->end())
				{
					graph.InsertDependency(codeUnitsCount, nodeMapping_->at(it->getID(astContext_)));
				}
			}			
		}
		
		return true;
	}
};