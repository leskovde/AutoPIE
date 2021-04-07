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
	DependencyGraph graph_;
	SkippedMapRef skippedNodes_;
	
	void RemoveFromSource(const clang::SourceRange range) const
	{		
		//llvm::outs() << RangeToString(astContext_, range);
		
		//llvm::outs() << localRewriter.getRewrittenText(range) << "\n";
		//llvm::outs() << range.printToString(astContext.getSourceManager()) << "\n";
		
		if (rewriter_)
		{
			llvm::outs() << "Removing node " << currentNode_ << ":\n" << RangeToString(astContext_, range);
			rewriter_->RemoveText(GetPrintableRange(GetPrintableRange(range, astContext_.getSourceManager()),
				astContext_.getSourceManager()));
			llvm::outs() << "\n";
		}
		else
		{
			llvm::outs() << "ERROR: Rewriter not set in the VariantPrintingASTVisitor!\n";
		}
	}

	bool ShouldBeRemoved()
	{
		if (!bitMask_[currentNode_])
		{
			// The bit is 0 => the node should not be present in the result.
			// However, if the parent is also set to 0, there will be an error when removing both.
			// Check for this case and remove the node only when all parents are set to 1.
			for(auto parent : graph_.GetParentNodes(currentNode_))
			{
				if (!bitMask_[parent])
				{
					return false;
				}
			}

			return true;
		}

		return false;
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

	void SetData(SkippedMapRef skippedNodes, DependencyGraph& graph)
	{
		skippedNodes_ = std::move(skippedNodes);
		graph_ = graph;
	}
	
	bool shouldTraversePostOrder() const
	{
		return true;
	}

	virtual bool VisitDecl(clang::Decl* decl)
	{
		if (llvm::isa<clang::TranslationUnitDecl>(decl) || llvm::isa<clang::VarDecl>(decl))
		{
			// Ignore the translation unit decl since it won't be manipulated with.
			// VarDecl have a DeclStmt counterpart that is easier to work with => avoid duplicates.
			return true;
		}
		
		if (skippedNodes_->find(currentNode_) == skippedNodes_->end() && ShouldBeRemoved())
		{
			const auto range = decl->getSourceRange();

			RemoveFromSource(range);
		}
		
		currentNode_++;
		
		return true;
	}

	bool VisitCallExpr(clang::CallExpr* expr)
	{
		if (skippedNodes_->find(currentNode_) == skippedNodes_->end() && ShouldBeRemoved())
		{
			const auto range = expr->getSourceRange();

			RemoveFromSource(range);
		}

		currentNode_++;
		
		return true;
	}
	
	virtual bool VisitStmt(clang::Stmt* stmt)
	{
		if (llvm::isa<clang::Expr>(stmt))
		{
			// Ignore expressions in general since they tend to be too small.
			return true;
		}
		
		if (skippedNodes_->find(currentNode_) == skippedNodes_->end() && ShouldBeRemoved())
		{
			const auto range = stmt->getSourceRange();

			RemoveFromSource(range);
		}

		currentNode_++;

		return true;
	}
};

class MappingASTVisitor : public clang::RecursiveASTVisitor<MappingASTVisitor>
{
	clang::ASTContext& astContext_;
	NodeMappingRef nodeMapping_;
	std::unordered_map<std::string, bool> snippetSet_;
	SkippedMapRef skippedNodes_ = std::make_shared<std::unordered_map<int, bool>>();

	bool InsertMapping(int astId, int myId, const std::string& snippet)
	{
		if (snippetSet_.find(snippet) != snippetSet_.end() || nodeMapping_->find(astId) != nodeMapping_->end())
		{
			// This node's code has already been processed.
			skippedNodes_->insert(std::pair<int, bool>(codeUnitsCount, true));
			
			return false;
		}

		return nodeMapping_->insert(std::pair<int, int>(astId, myId)).second;
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
		// TODO: Decide how to handle Decl subclasses with Stmt counterparts (e.g., VarDecl and DeclStmt) - handle all possible options.
		if (llvm::isa<clang::TranslationUnitDecl>(decl) || llvm::isa<clang::VarDecl>(decl))
		{
			// Ignore the translation unit decl since it won't be manipulated with.
			// VarDecl have a DeclStmt counterpart that is easier to work with => avoid duplicates.
			return true;
		}
		
		if (nodeMapping_->find(decl->getID()) == nodeMapping_->end())
		{
			const auto id = decl->getID();
			const auto typeName = decl->getDeclKindName();
			const auto codeSnippet = RangeToString(astContext_, decl->getSourceRange());
			
			llvm::outs() << "Node " << codeUnitsCount << ": Type " << typeName << "\n";
			
			if (InsertMapping(id, codeUnitsCount, codeSnippet))
			{
				snippetSet_[codeSnippet] = true;
				graph.InsertNodeDataForDebugging(codeUnitsCount, id, codeSnippet, typeName, "crimson");

				// Turns out declarations also have children.
				// TODO: Handle structs, enums, etc. by getting DeclContext and using specific methods.
				// http://clang-developers.42468.n3.nabble.com/How-to-traverse-all-children-FieldDecl-from-parent-CXXRecordDecl-td4045698.html
				const auto child = decl->getBody();
				
				if (child && nodeMapping_->find(child->getID(astContext_)) != nodeMapping_->end())
				{
					graph.InsertDependency(codeUnitsCount, nodeMapping_->at(child->getID(astContext_)));
				}
			}
			
			codeUnitsCount++;
		}
		else
		{
			llvm::outs() << "DEBUG: Attempted to visit node " << codeUnitsCount << " (already in the mapping).\n";
		}
		
		return true;
	}

	bool VisitCallExpr(clang::CallExpr* expr)
	{	
		if (nodeMapping_->find(expr->getID(astContext_)) == nodeMapping_->end())
		{
			const auto id = expr->getID(astContext_);
			const auto typeName = expr->getStmtClassName();
			const auto codeSnippet = RangeToString(astContext_, expr->getSourceRange());
			
			if (InsertMapping(id, codeUnitsCount, codeSnippet))
			{
				snippetSet_[codeSnippet] = true;
				graph.InsertNodeDataForDebugging(codeUnitsCount, id, codeSnippet, typeName, "goldenrod");
			}

			codeUnitsCount++;
		}
		else
		{
			llvm::outs() << "DEBUG: Attempted to visit node " << codeUnitsCount << " (already in the mapping).\n";
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
			const auto id = stmt->getID(astContext_);
			const auto typeName = stmt->getStmtClassName();
			const auto codeSnippet = RangeToString(astContext_, stmt->getSourceRange());
			
			llvm::outs() << "Node " << codeUnitsCount << ": Type " << typeName << "\n";
			
			if (InsertMapping(id, codeUnitsCount, codeSnippet))
			{
				snippetSet_[codeSnippet] = true;
				graph.InsertNodeDataForDebugging(codeUnitsCount, id, codeSnippet, typeName, "darkorchid");
			
				// Apparently only statements have children.
				for (auto it = stmt->child_begin(); it != stmt->child_end(); ++it)
				{
					if (*it && nodeMapping_->find(it->getID(astContext_)) != nodeMapping_->end())
					{
						graph.InsertDependency(codeUnitsCount, nodeMapping_->at(it->getID(astContext_)));
					}
				}
			}
			
			codeUnitsCount++;
		}
		else
		{
			llvm::outs() << "DEBUG: Attempted to visit node " << codeUnitsCount << " (already in the mapping).\n";
		}
		
		return true;
	}
};