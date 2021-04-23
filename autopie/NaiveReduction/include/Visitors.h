#ifndef VISITORS_H
#define VISITORS_H
#pragma once

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Rewrite/Core/Rewriter.h>

#include <utility>

#include "DependencyGraph.h"
#include "Helper.h"

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

/**
 * Traverses the AST and removes statements set by a given bitmask.\n
 * The current state of the source file is saved in a `Rewriter` instance.\n
 * The class requires additional setup using `SetData` and `Reset` methods to pass data
 * that could not be obtained during construction.
 */
class VariantPrintingASTVisitor final : public clang::RecursiveASTVisitor<VariantPrintingASTVisitor>
{
	clang::ASTContext& astContext_;

	BitMask bitMask_;
	int currentNode_ = 0; ///< The traversal order number.
	RewriterRef rewriter_;
	DependencyGraph graph_;
	SkippedMapRef skippedNodes_;

	/**
	 * Removes source code in a given range in the current rewriter.\n
	 * The range is only removed if the rewriter instance is valid.
	 *
	 * @param range The source range to be removed.
	 */
	void RemoveFromSource(const clang::SourceRange range) const
	{
		if (rewriter_)
		{
			out::Verb() << "Removing node " << currentNode_ << ":\n" << RangeToString(astContext_, range) << "\n";

			rewriter_->RemoveText(GetPrintableRange(GetPrintableRange(range, astContext_.getSourceManager()),
		                                        astContext_.getSourceManager()));
		}
		else
		{
			llvm::errs() << "ERROR: Rewriter not set in the VariantPrintingASTVisitor!\n";
		}
	}

	/**
	 * Determines whether a node should be removed based on the dependency graph.\n
	 * Since the traversal mode is set to postorder, it is possible that a snippet of source
	 * code is removed twice.\n
	 * E.g., a child's source range is removed first and then its parent's source range
	 * (which includes the child's source range) is removed as well, resulting a runtime error.\n
	 * This methods attempts to prevent these cases from happening by checking parent's bits
	 * in the bit mask.
	 *
	 * @return True if the statement is safe to be removed, false otherwise.
	 */
	bool ShouldBeRemoved()
	{
		if (!bitMask_[currentNode_])
		{
			// The bit is 0 => the node should not be present in the result.
			// However, if the parent is also set to 0, there will be an error when removing both.
			// Check for this case and remove the node only when all parents are set to 1.
			for (auto parent : graph_.GetParentNodes(currentNode_))
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
	explicit VariantPrintingASTVisitor(clang::CompilerInstance* ci) : astContext_(ci->getASTContext())
	{
	}

	/**
	 * Initializes data for a single iteration (i.e., one complete AST pass).
	 *
	 * @param mask The bitmask for the upcoming iteration specifying which nodes should be removed.
	 * @param rewriter The source code container from which nodes are removed. Each iteration requires
	 * a new, untouched rewriter.
	 */
	void Reset(const BitMask& mask, RewriterRef& rewriter)
	{
		currentNode_ = 0;
		bitMask_ = mask;
		rewriter_ = rewriter;
	}

	/**
	 * Initializes general data for all future passes.
	 *
	 * @param skippedNodes A container of AST nodes that should not be processed.
	 * @param graph The node dependency graph based on which nodes are removed.
	 */
	void SetData(SkippedMapRef skippedNodes, DependencyGraph& graph)
	{
		skippedNodes_ = std::move(skippedNodes);
		graph_ = graph;
	}

	/**
	 * Overrides the parent traversal mode.\n
	 * The traversal is changed from preorder to postorder.
	 */
	bool shouldTraversePostOrder() const
	{
		return true;
	}

	/**
	 * Overrides the parent visit method.\n
	 * Skips selected node types based on their importance.\n
	 * Removes valid nodes based on the provided bit mask and dependency graph.
	 */
	bool VisitDecl(clang::Decl* decl)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(decl->getBeginLoc()))
		//const auto loc = clang::FullSourceLoc(decl->getBeginLoc(), astContext_.getSourceManager());
		//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}
		
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

	/**
	 * Overrides the parent visit method.\n
	 * Removes valid nodes based on the provided bit mask and dependency graph.
	 */
	bool VisitCallExpr(clang::CallExpr* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
		//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
		//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}
		
		if (skippedNodes_->find(currentNode_) == skippedNodes_->end() && ShouldBeRemoved())
		{
			const auto range = expr->getSourceRange();

			RemoveFromSource(range);
		}

		currentNode_++;

		return true;
	}

	/**
	 * Overrides the parent visit method.\n
	 * Skips selected node types based on their importance.\n
	 * Removes valid nodes based on the provided bit mask and dependency graph.
	 */
	bool VisitStmt(clang::Stmt* stmt)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(stmt->getBeginLoc()))
		//const auto loc = clang::FullSourceLoc(stmt->getBeginLoc(), astContext_.getSourceManager());
		//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}
		
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

/**
 * Traverses the AST to analyze important nodes.\n
 * Splits the source code into code units based on the traversed nodes and their corresponding source ranges.\n
 * Creates a container of unimportant nodes that should be skipped during future traversals.\n
 * Sets the traversal order (by creating a node mapping and a skipped nodes list) for any future visitors.\n
 * Creates the dependency graph.
 */
class MappingASTVisitor final : public clang::RecursiveASTVisitor<MappingASTVisitor>
{
	int errorLine_;
	NodeMappingRef nodeMapping_;
	clang::ASTContext& astContext_;
	NodeMappingRef declNodeMapping_;
	std::unordered_map<std::string, bool> snippetSet_;
	SkippedMapRef skippedNodes_ = std::make_shared<std::unordered_map<int, bool>>();

	/**
	 * Maps an AST node based on its internal ID to the traversal order number on which the node was found.\n
	 * Mapping is not set if a node with the same ID or the same source range was previously processed.\n
	 * Any nodes present on the error-inducing line specified in the application's arguments are added
	 * to a separate container in the graph.
	 *
	 * @param astId The node identifier as given by the clang `node->getId()` method.
	 * @param snippet The source code corresponding to the processed node.
	 * @param line The line of the starting location of the corresponding source code.
	 */
	bool InsertMapping(const int astId, const std::string& snippet, const int line)
	{
		if (snippetSet_.find(snippet) != snippetSet_.end() || nodeMapping_->find(astId) != nodeMapping_->end())
		{
			// This node's code has already been processed.
			skippedNodes_->insert(std::pair<int, bool>(codeUnitsCount, true));

			return false;
		}

		if (errorLine_ == line)
		{
			graph.AddCriterionNode(codeUnitsCount);
		}

		return nodeMapping_->insert(std::pair<int, int>(astId, codeUnitsCount)).second;
	}

	void HandleDeclarationsInStatements(clang::Stmt* stmt) const
	{
		if (stmt != nullptr && llvm::isa<clang::DeclStmt>(stmt))
		{
			const auto declStmt = llvm::cast<clang::DeclStmt>(stmt);

			for (auto decl : declStmt->decls())
			{
				if (decl != nullptr)
				{
					const auto id = decl->getID();

					if (declNodeMapping_->find(id) == declNodeMapping_->end())
					{
						(*declNodeMapping_)[id] = codeUnitsCount;
					}
				}
			}
		}
	}

	void HandleVariableInstancesInStatements(clang::Stmt* stmt)
	{
		if (stmt != nullptr && llvm::isa<clang::Expr>(stmt))
		{
			const auto id = stmt->getID(astContext_);
			const auto typeName = stmt->getStmtClassName();
			const auto codeSnippet = RangeToString(astContext_, stmt->getSourceRange());
			const auto line = astContext_.getSourceManager().getSpellingLineNumber(stmt->getBeginLoc());
			
			for (auto it = stmt->child_begin(); it != stmt->child_end(); ++it)
			{
				const auto it_id = it->getID(astContext_);
				const auto it_typeName = it->getStmtClassName();
				const auto it_codeSnippet = RangeToString(astContext_, it->getSourceRange());
				const auto it_line = astContext_.getSourceManager().getSpellingLineNumber(it->getBeginLoc());
				
				// The statement / expression might contain a direct variable usage, e.g. `x = 5;`
				if (*it != nullptr && llvm::isa<clang::DeclRefExpr>(*it))
				{
					const auto parentID = llvm::cast<clang::DeclRefExpr>(*it)->getFoundDecl()->getID();

					if (declNodeMapping_->find(parentID) != declNodeMapping_->end())
					{
						graph.InsertDependency((*declNodeMapping_)[parentID], codeUnitsCount);
					}
				}
				// Sometimes the variable is hidden beneath an implicit cast.
				else if (*it != nullptr && llvm::isa<clang::ImplicitCastExpr>(*it))
				{
					HandleVariableInstancesInStatements(*it);
				}
				// Other times, the statement or expression might be using a variable inside an binary operator expression.
				// e.g. `assert(x == 5);`
				else if (*it != nullptr && llvm::isa<clang::BinaryOperator>(*it))
				{
					HandleVariableInstancesInStatements(*it);
				}
			}
		}
	}
	
	void ProcessDeclaration(clang::Decl* decl)
	{
		if (nodeMapping_->find(decl->getID()) == nodeMapping_->end())
		{
			const auto id = decl->getID();
			const auto typeName = decl->getDeclKindName();
			const auto codeSnippet = RangeToString(astContext_, decl->getSourceRange());
			const auto line = astContext_.getSourceManager().getSpellingLineNumber(decl->getBeginLoc());

			out::Verb() << "Node " << codeUnitsCount << ": Type " << typeName << "\n";

			if (InsertMapping(id, codeSnippet, line))
			{
				snippetSet_[codeSnippet] = true;
				graph.InsertNodeDataForDebugging(codeUnitsCount, id, codeSnippet, typeName, "crimson");

				if (llvm::cast<clang::FunctionDecl>(decl)->isMain())
				{
					graph.AddCriterionNode(codeUnitsCount);
				}

				// Turns out declarations also have children.
				// TODO(Denis): Handle structs, enums, etc. by getting DeclContext and using specific methods.
				// http://clang-developers.42468.n3.nabble.com/How-to-traverse-all-children-FieldDecl-from-parent-CXXRecordDecl-td4045698.html
				const auto child = decl->getBody();

				if (child != nullptr && nodeMapping_->find(child->getID(astContext_)) != nodeMapping_->end())
				{
					graph.InsertDependency(codeUnitsCount, nodeMapping_->at(child->getID(astContext_)));
				}
			}

			codeUnitsCount++;
		}
		else
		{
			out::Verb() << "DEBUG: Attempted to visit node " << codeUnitsCount << " (already in the mapping).\n";
		}
	}
	
	void ProcessCallExpression(clang::CallExpr* expr)
	{
		if (nodeMapping_->find(expr->getID(astContext_)) == nodeMapping_->end())
		{
			const auto id = expr->getID(astContext_);
			const auto typeName = expr->getStmtClassName();
			const auto codeSnippet = RangeToString(astContext_, expr->getSourceRange());
			const auto line = astContext_.getSourceManager().getSpellingLineNumber(expr->getBeginLoc());

			// Look for variable usages inside the current statement.
			// If the statement uses a previously declared variable, it should be dependent on that declaration.
			// e.g. `if (x < 2) { ... } `should depend on `int x = 0;`
			HandleVariableInstancesInStatements(expr);

			if (InsertMapping(id, codeSnippet, line))
			{
				snippetSet_[codeSnippet] = true;
				graph.InsertNodeDataForDebugging(codeUnitsCount, id, codeSnippet, typeName, "goldenrod");
			}

			codeUnitsCount++;
		}
		else
		{
			out::Verb() << "DEBUG: Attempted to visit node " << codeUnitsCount << " (already in the mapping).\n";
		}
	}

	void ProcessStatement(clang::Stmt* stmt)
	{
		if (nodeMapping_->find(stmt->getID(astContext_)) == nodeMapping_->end())
		{
			const auto id = stmt->getID(astContext_);
			const auto typeName = stmt->getStmtClassName();
			const auto codeSnippet = RangeToString(astContext_, stmt->getSourceRange());
			const auto line = astContext_.getSourceManager().getSpellingLineNumber(stmt->getBeginLoc());

			out::Verb() << "Node " << codeUnitsCount << ": Type " << typeName << "\n";

			if (InsertMapping(id, codeSnippet, line))
			{
				snippetSet_[codeSnippet] = true;
				graph.InsertNodeDataForDebugging(codeUnitsCount, id, codeSnippet, typeName, "darkorchid");

				HandleDeclarationsInStatements(stmt);
				//HandleVariableInstancesInStatements(stmt);

				// Apparently only statements have children.
				for (auto it = stmt->child_begin(); it != stmt->child_end(); ++it)
				{
					// Look for variable usages inside the current statement.
					// If the statement uses a previously declared variable, it should be dependent on that declaration.
					// e.g. `if (x < 2) { ... } `should depend on `int x = 0;`
					HandleVariableInstancesInStatements(*it);

					if (*it != nullptr && nodeMapping_->find(it->getID(astContext_)) != nodeMapping_->end())
					{
						graph.InsertDependency(codeUnitsCount, nodeMapping_->at(it->getID(astContext_)));
					}
				}
			}

			codeUnitsCount++;
		}
		else
		{
			out::Verb() << "DEBUG: Attempted to visit node " << codeUnitsCount << " (already in the mapping).\n";
		}
	}

public:
	int codeUnitsCount = 0; ///< The traversal order number.
	DependencyGraph graph = DependencyGraph();

	MappingASTVisitor(clang::CompilerInstance* ci, NodeMappingRef mapping, const int errorLine) : errorLine_(errorLine),
	                                                                                              nodeMapping_(std::move(mapping)),
	                                                                                              astContext_(ci->getASTContext())
	{
		declNodeMapping_ = std::make_shared<NodeMapping>();
	}

	/**
	 * Getter for the skipped nodes container.
	 *
	 * @return A container of the found unimportant nodes, specified by their traversal order numbers.
	 */
	[[nodiscard]] SkippedMapRef GetSkippedNodes() const
	{
		return skippedNodes_;
	}

	/**
	 * Overrides the parent traversal mode.\n
	 * The traversal is changed from preorder to postorder.
	 */
	bool shouldTraversePostOrder() const
	{
		return true;
	}

	/**
	 * Overrides the parent visit method.\n
	 * Skips selected node types based on their importance.\n
	 * Determines whether the node is worth visiting and creates an ID to traversal order number mapping for the node.\n
	 * Creates dependencies between the node and its children.
	 */
	bool VisitDecl(clang::Decl* decl)
	{		
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(decl->getBeginLoc()))
		//const auto loc = clang::FullSourceLoc(decl->getBeginLoc(), astContext_.getSourceManager());
		//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}
		
		// TODO(Denis): Decide how to handle Decl subclasses with Stmt counterparts (e.g., VarDecl and DeclStmt) - handle all possible options.
		if (llvm::isa<clang::TranslationUnitDecl>(decl) || llvm::isa<clang::VarDecl>(decl))
		{
			// Ignore the translation unit decl since it won't be manipulated with.
			// VarDecl have a DeclStmt counterpart that is easier to work with => avoid duplicates.
			return true;
		}

		ProcessDeclaration(decl);

		return true;
	}

	/**
	 * Overrides the parent visit method.\n
	 * Determines whether the node is worth visiting and creates an ID to traversal order number mapping for the node.\n
	 */
	bool VisitCallExpr(clang::CallExpr* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
		//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
		//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}
		
		ProcessCallExpression(expr);

		return true;
	}

	/**
	 * Overrides the parent visit method.\n
	 * Skips selected node types based on their importance.\n
	 * Determines whether the node is worth visiting and creates an ID to traversal order number mapping for the node.\n
	 * Creates dependencies between the node and its children.
	 */
	bool VisitStmt(clang::Stmt* stmt)
	{
		// TODO: Handle assignment expressions as nodes.
		
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(stmt->getBeginLoc()))
		//const auto loc = clang::FullSourceLoc(stmt->getBeginLoc(), astContext_.getSourceManager());
		//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}
		
		if (llvm::isa<clang::Expr>(stmt))
		{
			// Ignore expressions in general since they tend to be too small.
			return true;
		}

		ProcessStatement(stmt);

		return true;
	}
};
#endif
