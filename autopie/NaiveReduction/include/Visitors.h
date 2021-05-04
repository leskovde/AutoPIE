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
	int errorLineBackup_;
	RewriterRef rewriter_;
	DependencyGraph graph_;
	SkippedMapRef skippedNodes_;

	/**
	 * Removes source code in a given range in the current rewriter.\n
	 * The range is only removed if the rewriter instance is valid.
	 *
	 * @param range The source range to be removed.
	 */
	void RemoveFromSource(const clang::SourceRange range)
	{
		if (rewriter_)
		{
			out::Verb() << "Removing node " << currentNode_ << ":\n" << RangeToString(astContext_, range) << "\n";

			const auto printableRange = GetPrintableRange(GetPrintableRange(range, astContext_.getSourceManager()),
				astContext_.getSourceManager());

			const auto begin = astContext_.getSourceManager().getPresumedLineNumber(printableRange.getBegin());
			const auto snippet = GetSourceTextRaw(printableRange, astContext_.getSourceManager()).str();

			const auto lineBreaks = std::count_if(snippet.begin(), snippet.end(), [](const char c)
				{
					if (c == '\n')
					{
						return true;
					}

					return false;
				});

			
			if (begin < errorLineBackup_)
			{
				const int decrement = errorLineBackup_ >= begin + lineBreaks ? lineBreaks : errorLineBackup_ - begin;
				AdjustedErrorLine -= decrement;
			}
			
			rewriter_->RemoveText(printableRange);
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

	void ProcessRelevantExpression(clang::Expr* expr)
	{
		if (skippedNodes_->find(currentNode_) == skippedNodes_->end() && ShouldBeRemoved())
		{
			const auto range = expr->getSourceRange();

			RemoveFromSource(range);
		}

		currentNode_++;
	}

public:
	int AdjustedErrorLine;
	
	explicit VariantPrintingASTVisitor(clang::CompilerInstance* ci, const int errorLine) : astContext_(ci->getASTContext()), AdjustedErrorLine(errorLine), errorLineBackup_(errorLine)
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
		AdjustedErrorLine = errorLineBackup_;
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

#pragma region Expressions

	bool VisitAbstractConditionalOperator(clang::AbstractConditionalOperator* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr);

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

		ProcessRelevantExpression(expr);

		return true;
	}

	bool VisitBinaryOperator(clang::BinaryOperator* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		if (llvm::isa<clang::CompoundAssignOperator>(expr) || !expr->isAssignmentOp())
		{
			return true;
		}

		ProcessRelevantExpression(expr);

		return true;
	}
	
	bool VisitCompoundAssignOperator(clang::CompoundAssignOperator* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr);

		return true;
	}

	bool VisitChooseExpr(clang::ChooseExpr* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr);

		return true;
	}

	bool VisitCXXConstructExpr(clang::CXXConstructExpr* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr);

		return true;
	}

	bool VisitCXXDeleteExpr(clang::CXXDeleteExpr* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr);

		return true;
	}

	bool VisitCXXNewExpr(clang::CXXNewExpr* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr);

		return true;
	}

	bool VisitLambdaExpr(clang::LambdaExpr* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr);

		return true;
	}

	bool VisitStmtExpr(clang::StmtExpr* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr);

		return true;
	}

	bool VisitUnaryOperator(clang::UnaryOperator* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr);

		return true;
	}

#pragma endregion Expressions

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
	clang::ASTContext& astContext_;

	/**
	 * A map of integers that serves as a recognition tool for traversed nodes.\n
	 * Maps the node's AST ID (a unique replicable number that can be extracted from each node) to
	 * the traversal order number (the position of the node in the postfix traversal of this visitor)
	 * of the given node.
	 */
	NodeMappingRef nodeMapping_;
	

	/**
	 * A map of integers that serves as a recognition tool for declarations.\n
	 * Maps the node's AST ID (a unique replicable number that can be extracted from each node) to
	 * the traversal order number (the position of the node in the postfix traversal of this visitor)
	 * of a declaration node (a node that declares a function or a variable).
	 */
	NodeMappingRef declNodeMapping_;

	/**
	 * Keeps a list of found declaration references (i.e., variable usages).\n
	 * Each entry consists of a pair.\n
	 * The first value is the AST ID (a unique replicable number that can be extracted from each node) of
	 * the declaration node.\n
	 * The second value is the AST ID of the node in which the declared variable or function was referenced.
	 */
	std::vector<std::pair<int, int>> declReferences_;

	// TODO: Rework the snippet set (if a statement is repeated in the code, it does not get recognized).
	/**
	 * Stores a snippet of source code and its recognition flag.\n
	 * Upon discovering a node and successfully mapping it, the underlying source code is stored.\n
	 */
	std::unordered_map<std::string, bool> snippetSet_;

	/**
	 * Keeps note of which nodes should be skipped during traversal.\n
	 * If a node is a duplicate of an already processed node, it is added to this map.
	 */
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

	/**
	 * Checks whether the given statement represents a declaration.\n
	 * If it does, the node ID is mapped to the current traversal number in a separate declaration mapping.
	 *
	 * @param stmt The statement to be checked and mapped.
	 */
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

	/**
	 * Checks whether an expression is a declaration reference, i.e., a variable usage.\n
	 * If it does and the variable's declaration has been mapped, a dependency is created.\n
	 * The dependency states that the parent statement node (if encountered) is dependent on the declaring node.
	 *
	 * @param expr The expression to be checked for variable usages.
	 */
	void HandleVariableInstancesInExpressions(clang::Expr* expr)
	{
		// The statement / expression might contain a direct variable usage, e.g. `x = 5;`
		if (expr != nullptr && llvm::isa<clang::DeclRefExpr>(expr))
		{
			const auto parentId = llvm::cast<clang::DeclRefExpr>(expr)->getFoundDecl()->getID();			
			declReferences_.push_back(std::pair<int, int>(parentId, expr->getID(astContext_)));
		}
	}

	/**
	 * Determines whether a node given by the AST ID is in the subtree given by a statement node.\n
	 * The search is conducted in a DFS manner.
	 *
	 * @param stmt The statement node that is the root of the searched subtree.
	 * @param childId The AST ID of the node that is searched as a child.
	 * @return True if the node is in the given subtree, otherwise false.
	 */
	bool IsRecursiveChild(clang::Stmt* stmt, const int childId) const
	{
		if (stmt != nullptr)
		{
			for (auto& child : stmt->children())
			{
				if (child != nullptr && child->getID(astContext_) == childId)
				{
					return true;
				}

				if (IsRecursiveChild(child, childId))
				{
					return true;
				}
			}
		}

		return false;
	}

	/**
	 * Processes all found declaration references (i.e., variable usages) with respect to the current traversed statement.\n
	 * The current statement is given both by the `clang::Stmt*` parameter and by the `codeUnitsCount` traversal order number.\n
	 * All found declaration references are checked. In order to be processed, the declaring node has to be mapped.\n
	 * Additionally, the declaration reference's occurence node must be a recursive child of the current statement.\n
	 * If a declaration reference is successfully recognized, it is added as a variable dependency and removed from
	 * the list of unprocessed declaration references.
	 *
	 * @param stmt The current statement given by its `clang::Stmt*` instance.
	 */
	void CheckFoundDeclReferences(clang::Stmt* stmt)
	{
		std::vector<std::pair<int, int>> toBeKept;
		
		for (auto it = declReferences_.begin(); it != declReferences_.end(); ++it)
		{
			if (declNodeMapping_->find(it->first) != declNodeMapping_->end() && IsRecursiveChild(stmt, it->second))
			{
				graph.InsertVariableDependency((*declNodeMapping_)[it->first], codeUnitsCount);
			}
			else
			{
				toBeKept.push_back(*it);
			}
		}

		declReferences_ = toBeKept;
	}

	/**
	 * The body of VisitDecl after all invalid Decl types have been ruled out.\n
	 * Namely handles functions and maps their bodies to their declarations as a dependency.
	 *
	 * @param decl The current processed declaration.
	 */
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
					graph.InsertStatementDependency(codeUnitsCount, nodeMapping_->at(child->getID(astContext_)));
				}
			}

			codeUnitsCount++;
		}
		else
		{
			out::Verb() << "DEBUG: Attempted to visit node " << codeUnitsCount << " (already in the mapping).\n";
		}
	}

	/**
	 * The body of VisitCallExpr, VisitCompoundAssignOperator, and others, after all invalid Expr types
	 * have been ruled out.\n
	 * Checks call's variable dependencies and maps the node as valid.
	 *
	 * @param expr The current processed declaration.
	 */
	void ProcessRelevantExpression(clang::Expr* expr, const std::string& color)
	{
		if (nodeMapping_->find(expr->getID(astContext_)) == nodeMapping_->end())
		{
			const auto id = expr->getID(astContext_);
			const auto typeName = expr->getStmtClassName();
			const auto codeSnippet = RangeToString(astContext_, expr->getSourceRange());
			const auto line = astContext_.getSourceManager().getSpellingLineNumber(expr->getBeginLoc());

			out::Verb() << "Node " << codeUnitsCount << ": Type " << typeName << "\n";
			
			CheckFoundDeclReferences(expr);

			if (InsertMapping(id, codeSnippet, line))
			{
				snippetSet_[codeSnippet] = true;
				graph.InsertNodeDataForDebugging(codeUnitsCount, id, codeSnippet, typeName, color);
			}

			codeUnitsCount++;
		}
		else
		{
			out::Verb() << "DEBUG: Attempted to visit node " << codeUnitsCount << " (already in the mapping).\n";
		}
	}

	/**
	 * The body of VisitStmt after all invalid Stmt types have been ruled out.\n
	 * Checks both statement and variable dependencies, variable declarations, and maps the node as valid.
	 *
	 * @param stmt The current processed declaration.
	 */
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
				CheckFoundDeclReferences(stmt);

				// Apparently only statements have children.
				for (auto it = stmt->child_begin(); it != stmt->child_end(); ++it)
				{
					if (*it != nullptr && nodeMapping_->find(it->getID(astContext_)) != nodeMapping_->end())
					{
						graph.InsertStatementDependency(codeUnitsCount, nodeMapping_->at(it->getID(astContext_)));
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

#pragma region Expressions

	bool VisitAbstractConditionalOperator(clang::AbstractConditionalOperator* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr, "darkorchid");

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
		
		ProcessRelevantExpression(expr, "goldenrod");

		return true;
	}

	bool VisitBinaryOperator(clang::BinaryOperator* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		if (llvm::isa<clang::CompoundAssignOperator>(expr) || !expr->isAssignmentOp())
		{
			return true;
		}

		ProcessRelevantExpression(expr, "darkorchid");

		return true;
	}
	
	bool VisitCompoundAssignOperator(clang::CompoundAssignOperator* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr, "darkorchid");

		return true;
	}

	bool VisitChooseExpr(clang::ChooseExpr* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr, "darkorchid");

		return true;
	}

	bool VisitCXXConstructExpr(clang::CXXConstructExpr* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr, "darkorchid");

		return true;
	}

	bool VisitCXXDeleteExpr(clang::CXXDeleteExpr* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr, "darkorchid");

		return true;
	}

	bool VisitCXXNewExpr(clang::CXXNewExpr* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr, "darkorchid");

		return true;
	}
	
	/**
	 * Overrides the parent visit method.\n
	 * Serves primarily to find declaration references.
	 */
	bool VisitDeclRefExpr(clang::DeclRefExpr* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		// Look for variable usages inside the current expression.
		// If the statement uses a previously declared variable, it should be dependent on that declaration.
		// e.g. `if (x < 2) { ... } `should depend on `int x = 0;`
		HandleVariableInstancesInExpressions(expr);

		return true;
	}

	bool VisitLambdaExpr(clang::LambdaExpr* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr, "darkorchid");

		return true;
	}

	bool VisitStmtExpr(clang::StmtExpr* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr, "darkorchid");

		return true;
	}

	bool VisitUnaryOperator(clang::UnaryOperator* expr)
	{
		// Skip included files.
		if (!astContext_.getSourceManager().isInMainFile(expr->getBeginLoc()))
			//const auto loc = clang::FullSourceLoc(expr->getBeginLoc(), astContext_.getSourceManager());
			//if (loc.isValid() && loc.isInSystemHeader())
		{
			return true;
		}

		ProcessRelevantExpression(expr, "darkorchid");

		return true;
	}

#pragma endregion Expressions
	
	/**
	 * Overrides the parent visit method.\n
	 * Skips selected node types based on their importance.\n
	 * Determines whether the node is worth visiting and creates an ID to traversal order number mapping for the node.\n
	 * Creates dependencies between the node and its children.
	 */
	bool VisitStmt(clang::Stmt* stmt)
	{
		// TODO: Handle assignment expressions as nodes.
		// TODO: Make sure the processing flow in this method and in ProcessStatement is correct.
		
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
