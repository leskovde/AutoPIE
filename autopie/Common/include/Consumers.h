#ifndef CONSUMERS_H
#define CONSUMERS_H
#pragma once

#include <clang/AST/ASTConsumer.h>

#include "DependencyGraph.h"
#include "Visitors.h"
#include "Streams.h"

namespace Common
{
	/**
	 * Prepares and dispatches the `VariantPrintingASTVisitor`, prints its output.\n
	 * Serves as a middle man for data transactions.
	 */
	class VariantPrintingASTConsumer final : public clang::ASTConsumer
	{
		VariantPrintingASTVisitorRef visitor_;

	public:
		explicit VariantPrintingASTConsumer(clang::CompilerInstance* ci, int errorLine) : visitor_(std::make_unique<VariantPrintingASTVisitor>(ci, errorLine))
		{
		}

		/**
		 * Dispatches the visitor to the root node.\n
		 * The visitor is fed with input data and output references so that the output can be extracted after the AST pass.\n
		 * A variant is generated based on the input bitmask.\n
		 * The generated variant is dumped to the specified file.
		 *
		 * @param context The AST context.
		 * @param fileName The file to which the output should be written.
		 * @param bitMask The specification of which nodes should be kept and which should be removed.
		 */
		void HandleTranslationUnit(clang::ASTContext& context, const std::string& fileName, const BitMask& bitMask) const
		{
			auto rewriter = std::make_shared<clang::Rewriter>(context.getSourceManager(), context.getLangOpts());

			visitor_->Reset(bitMask, rewriter);
			visitor_->TraverseDecl(context.getTranslationUnitDecl());

			out::Verb() << "Variant after iteration:\n";

			if (Verbose)
			{
				rewriter->getEditBuffer(context.getSourceManager().getMainFileID()).write(llvm::errs());

			}
			out::Verb() << "\n";

			std::error_code errorCode;
			llvm::raw_fd_ostream outFile(fileName, errorCode, llvm::sys::fs::F_None);

			rewriter->getEditBuffer(context.getSourceManager().getMainFileID()).write(outFile);
			outFile.close();
		}

		/**
		 * Passed instances of members that could not be initialized previously to the visitor.
		 *
		 * @param skippedNodes A container of nodes that should be skipped during the traversal generated by the `MappingVisitor`.
		 * @param graph The node dependency graph generated by the `MappingVisitor`.
		 */
		void SetData(SkippedMapRef skippedNodes, DependencyGraph graph) const
		{
			visitor_->SetData(std::move(skippedNodes), graph);
		}

		/**
		 * After the visitor's traversal is complete, the error-inducing line number is updated.
		 *
		 * @return The new presumed line on which the error should be thrown.
		 */
		int GetAdjustedErrorLine() const
		{
			return visitor_->AdjustedErrorLine;
		}
	};
	
	/**
	 * Dispatches the `MappingASTVisitor` and collects its output.\n
	 * Serves as a middle man for data transactions.
	 */
	class DependencyMappingASTConsumer final : public clang::ASTConsumer
	{
		NodeMappingRef nodeMapping_;
		MappingASTVisitorRef mappingVisitor_;
		GlobalContext& globalContext_;
		const int iteration_{0};

	public:
		DependencyMappingASTConsumer(clang::CompilerInstance* ci, GlobalContext& context) : globalContext_(context)
		{
			nodeMapping_ = std::make_shared<NodeMapping>();
			mappingVisitor_ = std::make_unique<MappingASTVisitor>(ci, nodeMapping_,
				globalContext_.parsedInput.errorLocation.lineNumber);
		}

		DependencyMappingASTConsumer(clang::CompilerInstance* ci, GlobalContext& context, const int iteration) : globalContext_(context), iteration_(iteration)
		{
			nodeMapping_ = std::make_shared<NodeMapping>();
			mappingVisitor_ = std::make_unique<MappingASTVisitor>(ci, nodeMapping_,
				globalContext_.parsedInput.errorLocation.lineNumber);
		}

		/**
		 * Dispatches the visitor to the root node.\n
		 * The visitors' output is then saved and printed or dumped to a `.dot` file.
		 *
		 * @param context The AST context.
		 */
		void HandleTranslationUnit(clang::ASTContext& context) override
		{
			mappingVisitor_->TraverseDecl(context.getTranslationUnitDecl());

			out::Verb() << "DEBUG: AST nodes counted: " << mappingVisitor_->codeUnitsCount << ", AST nodes actual: " <<
				nodeMapping_->size() << "\n";

			if (Verbose)
			{
				mappingVisitor_->graph.PrintGraphForDebugging();
			}

			if (globalContext_.parsedInput.dumpDot && globalContext_.currentEpoch == 0)
			{
				const auto dotFileOutput = VisualsFolder + std::string("dotDump_") + std::to_string(iteration_) + "_" + GetFileName(globalContext_.parsedInput.errorLocation.filePath) + ".dot";
				mappingVisitor_->graph.DumpDot(dotFileOutput);
			}
		}

		/**
		 * Getter for the visitor's code unit count data.
		 *
		 * @return The number of code units (important nodes) encountered in the source file.
		 */
		[[nodiscard]] int GetCodeUnitsCount() const
		{
			return nodeMapping_->size();
		}

		/**
		 * Getter for the visitor's graph data.
		 *
		 * @return A copy of the created node dependency graph.
		 */
		[[nodiscard]] DependencyGraph GetDependencyGraph() const
		{
			return mappingVisitor_->graph;
		}

		/**
		 * Getter for the visitor's skipped nodes container.
		 *
		 * @return A pointer to the skipped nodes container.
		 */
		[[nodiscard]] SkippedMapRef GetSkippedNodes() const
		{
			return mappingVisitor_->GetSkippedNodes();
		}
	};
}

#endif