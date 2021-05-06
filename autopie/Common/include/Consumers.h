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
	 * Dispatches the `MappingASTVisitor` and collects its output.\n
	 * Serves as a middle man for data transactions.
	 */
	class DependencyMappingASTConsumer final : public clang::ASTConsumer
	{
		NodeMappingRef nodeMapping_;
		MappingASTVisitorRef mappingVisitor_;
		GlobalContext& globalContext_;

	public:
		DependencyMappingASTConsumer(clang::CompilerInstance* ci, GlobalContext& context) : globalContext_(context)
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
				mappingVisitor_->graph.DumpDot(globalContext_.parsedInput.errorLocation.filePath);
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