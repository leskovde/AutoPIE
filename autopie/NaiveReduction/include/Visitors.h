#ifndef VISITORS_NAIVE_H
#define VISITORS_NAIVE_H
#pragma once

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Rewrite/Core/Rewriter.h>

#include <utility>

#include "../../Common/include/DependencyGraph.h"
#include "../../Common/include/Helper.h"
#include "../../Common/include/Visitors.h"

namespace Naive
{
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

				RemoveFromSource(range);
			}

			currentNode_++;

			return true;
		}
	};
}

#endif