#ifndef VISITORS_H
#define VISITORS_H
#pragma once

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Rewrite/Core/Rewriter.h>

#include <utility>

#include "DependencyGraph.h"
#include "Helper.h"

namespace Common
{
	class MappingASTVisitor;

	using NodeMapping = std::unordered_map<int, int>;
	using NodeMappingRef = std::shared_ptr<std::unordered_map<int, int>>;
	using MappingASTVisitorRef = std::unique_ptr<MappingASTVisitor>;
	using SkippedMapRef = std::shared_ptr<std::unordered_map<int, bool>>;

	class VariantPrintingASTVisitor;

	using RewriterRef = std::shared_ptr<clang::Rewriter>;
	using VariantPrintingASTVisitorRef = std::unique_ptr<VariantPrintingASTVisitor>;

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
		std::vector<int> errorLineBackups_;
		RewriterRef rewriter_;
		DependencyGraph graph_;
		SkippedMapRef skippedNodes_;

		/**
		 * Removes source code in a given range in the current rewriter.\n
		 * The range is only removed if the rewriter instance is valid.
		 *
		 * @param range The source range to be removed.
		 * @param replace Specifies whether the range should be replaced with a single semicolon.
		 */
		void RemoveFromSource(const clang::SourceRange range, const bool replace = false)
		{
			if (rewriter_)
			{
				Out::Verb() << "Removing node " << currentNode_ << ":\n" << RangeToString(astContext_, range) << "\n";

				const auto printableRange = GetPrintableRange(GetPrintableRange(range, astContext_.getSourceManager()),
					astContext_.getSourceManager());

				const auto begin = astContext_.getSourceManager().getSpellingLineNumber(printableRange.getBegin());
				const auto snippet = GetSourceTextRaw(printableRange, astContext_.getSourceManager()).str();

				const auto lineBreaks = std::count_if(snippet.begin(), snippet.end(), [](const char c)
					{
						if (c == '\n')
						{
							return true;
						}

						return false;
					});

				for (auto i = 0; i < adjustedErrorLines.size(); i++)
				{
					if (begin < errorLineBackups_[i])
					{
						const int decrement = errorLineBackups_[i] >= begin + lineBreaks ? lineBreaks : errorLineBackups_[i] - begin;
						adjustedErrorLines[i] -= decrement;
					}
				}
				
				if (replace)
				{
					rewriter_->ReplaceText(printableRange, ";");
				}
				else
				{
					rewriter_->RemoveText(printableRange);
				}
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

		/**
		 * Check whether the current processed file is an included one.\n
		 * In case it is, the traversal of the declaration node is skipped.
		 *
		 * @param decl The declaration node whose location is checked.
		 * @return True if the node is in an included file, false otherwise.
		 */
		bool SkipIncludeDecl(clang::Decl* decl) const
		{
			// Skip included files.
			if (!astContext_.getSourceManager().isInMainFile(decl->getBeginLoc()))
				//const auto loc = clang::FullSourceLoc(decl->getBeginLoc(), astContext_.getSourceManager());
				//if (loc.isValid() && loc.isInSystemHeader())
			{
				return true;
			}
			return false;
		}

		/**
		 * Check whether the current processed file is an included one.\n
		 * In case it is, the traversal of the statement/expression node is skipped.
		 *
		 * @param stmt The statement/expression node whose location is checked.
		 * @return True if the node is in an included file, false otherwise.
		 */
		bool SkipIncludeStmt(clang::Stmt* stmt) const
		{
			// Skip included files.
			if (!astContext_.getSourceManager().isInMainFile(stmt->getBeginLoc()))
				//const auto loc = clang::FullSourceLoc(stmt->getBeginLoc(), astContext_.getSourceManager());
				//if (loc.isValid() && loc.isInSystemHeader())
			{
				return true;
			}
			return false;
		}

	public:
		std::vector<int> adjustedErrorLines;

		VariantPrintingASTVisitor(clang::CompilerInstance* ci, const int errorLine) : astContext_(ci->getASTContext()), errorLineBackups_({ errorLine }), adjustedErrorLines({ errorLine })
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
			adjustedErrorLines = errorLineBackups_;
		}

		/**
		 * Initializes general data for all future passes.
		 *
		 * @param skippedNodes A container of AST nodes that should not be processed.
		 * @param graph The node dependency graph based on which nodes are removed.
		 * @param errorLines A list of potential error-inducing lines (LLDB workaround).
		 */
		void SetData(SkippedMapRef skippedNodes, DependencyGraph& graph, std::vector<int>& errorLines)
		{
			skippedNodes_ = std::move(skippedNodes);
			graph_ = graph;
			errorLineBackups_ = errorLines;
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
			if (SkipIncludeDecl(decl))
			{
				return true;
			}

			if (llvm::isa<clang::TranslationUnitDecl>(decl) || llvm::isa<clang::VarDecl>(decl) || llvm::isa<clang::AccessSpecDecl>(decl))
			{
				// Ignore the translation unit decl since it won't be manipulated with.
				// VarDecl have a DeclStmt counterpart that is easier to work with => avoid duplicates.
				// There is no need to remove access specifiers (AccessSpecDecl) for now.
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
			if (SkipIncludeStmt(expr))
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
			if (SkipIncludeStmt(expr))
			{
				return true;
			}

			ProcessRelevantExpression(expr);

			return true;
		}

		bool VisitBinaryOperator(clang::BinaryOperator* expr)
		{
			if (SkipIncludeStmt(expr))
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
			if (SkipIncludeStmt(expr))
			{
				return true;
			}

			ProcessRelevantExpression(expr);

			return true;
		}

		bool VisitChooseExpr(clang::ChooseExpr* expr)
		{
			if (SkipIncludeStmt(expr))
			{
				return true;
			}

			ProcessRelevantExpression(expr);

			return true;
		}

		bool VisitCXXDeleteExpr(clang::CXXDeleteExpr* expr)
		{
			if (SkipIncludeStmt(expr))
			{
				return true;
			}

			ProcessRelevantExpression(expr);

			return true;
		}

		bool VisitCXXNewExpr(clang::CXXNewExpr* expr)
		{
			if (SkipIncludeStmt(expr))
			{
				return true;
			}

			ProcessRelevantExpression(expr);

			return true;
		}

		bool VisitLambdaExpr(clang::LambdaExpr* expr)
		{
			if (SkipIncludeStmt(expr))
			{
				return true;
			}

			ProcessRelevantExpression(expr);

			return true;
		}

		bool VisitStmtExpr(clang::StmtExpr* expr)
		{
			if (SkipIncludeStmt(expr))
			{
				return true;
			}

			ProcessRelevantExpression(expr);

			return true;
		}

		bool VisitUnaryOperator(clang::UnaryOperator* expr)
		{
			if (SkipIncludeStmt(expr))
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
			if (SkipIncludeStmt(stmt))
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

				if (llvm::isa<clang::CompoundStmt>(stmt) || llvm::isa<clang::NullStmt>(stmt))
				{
					// Replace compound statements with a null statement instead of removing them.
					// Removing a compound statement could result in an semantically invalid variant.
					RemoveFromSource(range, true);
				}
				else
				{
					RemoveFromSource(range);
				}
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
		bool criterionFound_ = false;
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

		/**
		 * Keeps note of which nodes should be skipped during traversal.\n
		 * If a node is a duplicate of an already processed node, it is added to this map.
		 */
		SkippedMapRef skippedNodes_ = std::make_shared<std::unordered_map<int, bool>>();

		std::unordered_map<int, bool> childStatements_;

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
			if (nodeMapping_->find(astId) != nodeMapping_->end())
			{
				// This node's code has already been processed.
				skippedNodes_->insert(std::pair<int, bool>(codeUnitsCount, true));

				return false;
			}

			if (errorLine_ == line)
			{
				graph.AddCriterionNode(codeUnitsCount);
				criterionFound_ = true;
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

		std::vector<clang::Stmt*> GetChildrenRecursively(clang::Stmt* stmt) const
		{
			auto children = std::vector<clang::Stmt*>();

			for (auto it = stmt->child_begin(); it != stmt->child_end(); ++it)
			{
				if (*it != nullptr)
				{
					if (nodeMapping_->find(it->getID(astContext_)) != nodeMapping_->end())
					{
						children.push_back(*it);
					}
					
					auto recursiveChildren = GetChildrenRecursively(*it);
					children.insert(children.end(), recursiveChildren.begin(), recursiveChildren.end());
				}
			}

			return children;
		}
		
		void CreateChildDependencies(clang::Stmt* stmt)
		{
			for (auto& child : GetChildrenRecursively(stmt))
			{
				if (child != nullptr && childStatements_.find(child->getID(astContext_)) != childStatements_.end())
				{
					graph.InsertStatementDependency(codeUnitsCount, nodeMapping_->at(child->getID(astContext_)));
					childStatements_.erase(child->getID(astContext_));
				}
			}
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

				Out::Verb() << "Node " << codeUnitsCount << ": Type " << typeName << "\n";

				if (InsertMapping(id, codeSnippet, line))
				{
					graph.InsertNodeDataForDebugging(codeUnitsCount, id, codeSnippet, typeName, "crimson");

					if (llvm::isa<clang::FunctionDecl>(decl) && llvm::cast<clang::FunctionDecl>(decl)->isMain())
					{
						graph.AddCriterionNode(codeUnitsCount);
					}

					// Map the child as dependency.
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
				Out::Verb() << "DEBUG: Attempted to visit node " << codeUnitsCount << " (already in the mapping).\n";
			}
		}

		/**
		 * The body of VisitCallExpr, VisitCompoundAssignOperator, and others, after all invalid Expr types
		 * have been ruled out.\n
		 * Checks call's variable dependencies and maps the node as valid.
		 *
		 * @param expr The current processed declaration.
		 * @param color The color with which the node should be drawn in GraphViz visualization.
		 */
		void ProcessRelevantExpression(clang::Expr* expr, const std::string& color)
		{
			if (nodeMapping_->find(expr->getID(astContext_)) == nodeMapping_->end())
			{
				const auto id = expr->getID(astContext_);
				const auto typeName = expr->getStmtClassName();
				const auto codeSnippet = RangeToString(astContext_, expr->getSourceRange());
				const auto line = astContext_.getSourceManager().getSpellingLineNumber(expr->getBeginLoc());

				Out::Verb() << "Node " << codeUnitsCount << ": Type " << typeName << "\n";

				CheckFoundDeclReferences(expr);

				if (InsertMapping(id, codeSnippet, line))
				{
					graph.InsertNodeDataForDebugging(codeUnitsCount, id, codeSnippet, typeName, color);

					// Map children as dependencies.
					CreateChildDependencies(expr);

					// This expression was just found and might be a child of another statement.
					// Add it to the unmapped children container.
					childStatements_.insert(std::pair<int, bool>(id, true));
				}

				codeUnitsCount++;
			}
			else
			{
				Out::Verb() << "DEBUG: Attempted to visit node " << codeUnitsCount << " (already in the mapping).\n";
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

				Out::Verb() << "Node " << codeUnitsCount << ": Type " << typeName << "\n";

				if (InsertMapping(id, codeSnippet, line))
				{
					graph.InsertNodeDataForDebugging(codeUnitsCount, id, codeSnippet, typeName, "darkorchid");

					HandleDeclarationsInStatements(stmt);
					CheckFoundDeclReferences(stmt);

					// Map visited children as dependencies.
					CreateChildDependencies(stmt);

					// This expression was just found and might be a child of another statement.
					// Add it to the unmapped children container.
					childStatements_.insert(std::pair<int, bool>(id, true));
				}

				codeUnitsCount++;
			}
			else
			{
				Out::Verb() << "DEBUG: Attempted to visit node " << codeUnitsCount << " (already in the mapping).\n";
			}
		}

		/**
		 * Check whether the current processed file is an included one.\n
		 * In case it is, the traversal of the declaration node is skipped.
		 *
		 * @param decl The declaration node whose location is checked.
		 * @return True if the node is in an included file, false otherwise.
		 */
		bool SkipIncludeDecl(clang::Decl* decl) const
		{
			// Skip included files.
			if (!astContext_.getSourceManager().isInMainFile(decl->getBeginLoc()))
				//const auto loc = clang::FullSourceLoc(decl->getBeginLoc(), astContext_.getSourceManager());
				//if (loc.isValid() && loc.isInSystemHeader())
			{
				return true;
			}
			return false;
		}

		/**
		 * Check whether the current processed file is an included one.\n
		 * In case it is, the traversal of the statement/expression node is skipped.
		 *
		 * @param stmt The statement/expression node whose location is checked.
		 * @return True if the node is in an included file, false otherwise.
		 */
		bool SkipIncludeStmt(clang::Stmt* stmt) const
		{
			// Skip included files.
			if (!astContext_.getSourceManager().isInMainFile(stmt->getBeginLoc()))
				//const auto loc = clang::FullSourceLoc(stmt->getBeginLoc(), astContext_.getSourceManager());
				//if (loc.isValid() && loc.isInSystemHeader())
			{
				return true;
			}
			return false;
		}

	public:
		int codeUnitsCount = 0; ///< The traversal order number.
		std::vector<int> errorLines;
		DependencyGraph graph = DependencyGraph();

		MappingASTVisitor(clang::CompilerInstance* ci, NodeMappingRef mapping, const int errorLine) : errorLine_(errorLine),
			astContext_(ci->getASTContext()),
			nodeMapping_(std::move(mapping))
		{
			declNodeMapping_ = std::make_shared<NodeMapping>();
			errorLines.push_back(errorLine);
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

#pragma region Declarations

		/**
		 * Overrides the parent visit method.\n
		 * Skips selected node types based on their importance.\n
		 * Determines whether the node is worth visiting and creates an ID to traversal order number mapping for the node.\n
		 * Creates dependencies between the node and its children.
		 */
		bool VisitDecl(clang::Decl* decl)
		{
			if (SkipIncludeDecl(decl))
			{
				return true;
			}

			if (llvm::isa<clang::TranslationUnitDecl>(decl) || llvm::isa<clang::VarDecl>(decl) || llvm::isa<clang::AccessSpecDecl>(decl))
			{
				// Ignore the translation unit decl since it won't be manipulated with.
				// VarDecl have a DeclStmt counterpart that is easier to work with => avoid duplicates.
				// There is no need to remove access specifiers (AccessSpecDecl) for now.
				return true;
			}

			ProcessDeclaration(decl);

			return true;
		}

		bool VisitFunctionDecl(clang::FunctionDecl* decl)
		{
			if (SkipIncludeDecl(decl))
			{
				return true;
			}

			if (criterionFound_)
			{
				const auto range = GetPrintableRange(GetPrintableRange(decl->getSourceRange(), astContext_.getSourceManager()),
					astContext_.getSourceManager());

				const auto startingLine = astContext_.getSourceManager().getSpellingLineNumber(range.getBegin());
				const auto endingLine = astContext_.getSourceManager().getSpellingLineNumber(range.getEnd());

				if (decl->hasBody() && decl->getBody() != nullptr)
				{
					const auto bodyRange = GetPrintableRange(GetPrintableRange(decl->getBody()->getSourceRange(), astContext_.getSourceManager()),
						astContext_.getSourceManager());

					const auto bodyStartingLine = astContext_.getSourceManager().getSpellingLineNumber(bodyRange.getBegin());
					const auto bodyEndingLine = astContext_.getSourceManager().getSpellingLineNumber(bodyRange.getEnd());

					for (auto i = startingLine; i <= bodyStartingLine; i++)
					{
						errorLines.push_back(i);
					}

					for (auto i = bodyEndingLine; i <= endingLine; i++)
					{
						errorLines.push_back(i);
					}
				}
				else
				{
					errorLines.push_back(startingLine);
				}
				
				criterionFound_ = false;
			}
			
			return true;

		}

		/**
		 * Overrides the parent visit method.\n
		 * Visits a RecordDecl and traverses its children.\n
		 * Those children that have already been traverses will be mapped as dependencies
		 * of the current node.
		 */
		bool VisitRecordDecl(clang::RecordDecl* decl)
		{
			if (SkipIncludeDecl(decl))
			{
				return true;
			}

			// Map fields as dependencies.
			for (auto it = decl->field_begin(); it != decl->field_end(); ++it)
			{
				if (*it != nullptr && nodeMapping_->find(it->getID()) != nodeMapping_->end())
				{
					graph.InsertStatementDependency(codeUnitsCount - 1, nodeMapping_->at(it->getID()));
				}
			}

			return true;
		}

		bool VisitCXXRecordDecl(clang::CXXRecordDecl* decl)
		{
			if (SkipIncludeDecl(decl))
			{
				return true;
			}

			// Map constructors as dependencies.
			for (auto it : decl->ctors())
			{
				if (it != nullptr && nodeMapping_->find(it->getID()) != nodeMapping_->end())
				{
					graph.InsertStatementDependency(codeUnitsCount - 1, nodeMapping_->at(it->getID()));
				}
			}

			// Map methods as dependencies.
			for (auto it : decl->methods())
			{
				if (it != nullptr && nodeMapping_->find(it->getID()) != nodeMapping_->end())
				{
					graph.InsertStatementDependency(codeUnitsCount - 1, nodeMapping_->at(it->getID()));
				}
			}

			return true;
		}

		/**
		 * Overrides the parent visit method.\n
		 * Visits a RecordDecl and traverses its children.\n
		 * Those children that have already been traverses will be mapped as dependencies
		 * of the current node.
		 */
		bool VisitEnumDecl(clang::EnumDecl* decl)
		{
			if (SkipIncludeDecl(decl))
			{
				return true;
			}

			// Map constants as dependencies.
			for (auto it = decl->enumerator_begin(); it != decl->enumerator_end(); ++it)
			{
				if (*it != nullptr && nodeMapping_->find(it->getID()) != nodeMapping_->end())
				{
					graph.InsertStatementDependency(codeUnitsCount - 1, nodeMapping_->at(it->getID()));
				}
			}

			return true;
		}

#pragma endregion Declarations

#pragma region Expressions

		bool VisitAbstractConditionalOperator(clang::AbstractConditionalOperator* expr)
		{
			if (SkipIncludeStmt(expr))
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
			if (SkipIncludeStmt(expr))
			{
				return true;
			}

			ProcessRelevantExpression(expr, "goldenrod");

			const auto decl = expr->getCalleeDecl();

			if (decl != nullptr) 
			{
				if (nodeMapping_->find(decl->getID()) != nodeMapping_->end())
				{
					graph.InsertVariableDependency(nodeMapping_->at(decl->getID()), codeUnitsCount - 1);
				}
			}
			
			return true;
		}

		bool VisitBinaryOperator(clang::BinaryOperator* expr)
		{
			if (SkipIncludeStmt(expr))
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
			if (SkipIncludeStmt(expr))
			{
				return true;
			}

			ProcessRelevantExpression(expr, "darkorchid");

			return true;
		}

		bool VisitChooseExpr(clang::ChooseExpr* expr)
		{
			if (SkipIncludeStmt(expr))
			{
				return true;
			}

			ProcessRelevantExpression(expr, "darkorchid");

			return true;
		}

		bool VisitCXXDeleteExpr(clang::CXXDeleteExpr* expr)
		{
			if (SkipIncludeStmt(expr))
			{
				return true;
			}

			ProcessRelevantExpression(expr, "darkorchid");

			return true;
		}

		bool VisitCXXNewExpr(clang::CXXNewExpr* expr)
		{
			if (SkipIncludeStmt(expr))
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
			if (SkipIncludeStmt(expr))
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
			if (SkipIncludeStmt(expr))
			{
				return true;
			}

			ProcessRelevantExpression(expr, "darkorchid");

			return true;
		}

		bool VisitStmtExpr(clang::StmtExpr* expr)
		{
			if (SkipIncludeStmt(expr))
			{
				return true;
			}

			ProcessRelevantExpression(expr, "darkorchid");

			return true;
		}

		bool VisitUnaryOperator(clang::UnaryOperator* expr)
		{
			if (SkipIncludeStmt(expr))
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
			if (SkipIncludeStmt(stmt))
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
}

#endif