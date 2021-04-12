#ifndef CONSUMERS_H
#define CONSUMERS_H
#pragma once

#include <clang/AST/ASTConsumer.h>

#include <utility>

#include "Context.h"
#include "DependencyGraph.h"
#include "Visitors.h"

/**
 * Prepares and dispatches the `VariantPrintingASTVisitor`, prints its output.\n
 * Serves as a middle man for data transactions.
 */
class VariantPrintingASTConsumer final : public clang::ASTConsumer
{
	VariantPrintingASTVisitorRef visitor_;

public:
	explicit VariantPrintingASTConsumer(clang::CompilerInstance* ci) : visitor_(std::make_unique<VariantPrintingASTVisitor>(ci))
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

		llvm::outs() << "Variant after iteration:\n";
		rewriter->getEditBuffer(context.getSourceManager().getMainFileID()).write(llvm::errs());
		llvm::outs() << "\n";

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
		// TODO(Denis): Handle specific node types instead of just Stmt, Decl and Expr.

		mappingVisitor_->TraverseDecl(context.getTranslationUnitDecl());

		llvm::outs() << "DEBUG: AST nodes counted: " << mappingVisitor_->codeUnitsCount << ", AST nodes actual: " <<
			nodeMapping_->size() << "\n";

		mappingVisitor_->graph.PrintGraphForDebugging();

		if (globalContext_.parsedInput.dumpDot)
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

/**
 * Unifies other consumers and uses them to describe the variant generation logic.\n
 * Single `HandleTranslationUnit` generates all source code variants.
 */
class VariantGenerationConsumer final : public clang::ASTConsumer
{
	DependencyMappingASTConsumer mappingConsumer_;
	VariantPrintingASTConsumer printingConsumer_;

public:
	VariantGenerationConsumer(clang::CompilerInstance* ci, GlobalContext& context) : mappingConsumer_(ci, context),
	                                                                                 printingConsumer_(ci)
	{
	}

	/**
	 * Runs two consumer steps to generate all possible variants.\n
	 * Firstly, a mapping consumer is called to analyze AST nodes and create a traversal order.\n
	 * Secondly, a bitmask based on the number of found code units is created.
	 * Each bit in the bitmask represents a node based on the traversal order.
	 * Setting a bit to true translates to the corresponding node being present in the output.
	 * Setting a bit to false results in the deletion of the corresponding node.\n
	 * Lastly, all possible bitmask variants are generated and the variant printing consumer is called
	 * for each bitmask variant.
	 *
	 * @param context The AST context.
	 */
	void HandleTranslationUnit(clang::ASTContext& context) override
	{
		mappingConsumer_.HandleTranslationUnit(context);
		const auto numberOfCodeUnits = mappingConsumer_.GetCodeUnitsCount();

		printingConsumer_.SetData(mappingConsumer_.GetSkippedNodes(), mappingConsumer_.GetDependencyGraph());

		auto bitMask = BitMask(numberOfCodeUnits);
		auto dependencies = mappingConsumer_.GetDependencyGraph();

		llvm::outs() << "Maximum expected variants: " << pow(2, numberOfCodeUnits) << "\n";

		auto variantsCount = 0;

		while (!IsFull(bitMask))
		{
			Increment(bitMask);

			if (IsValid(bitMask, dependencies))
			{
				variantsCount++;

				if (variantsCount % 50 == 0)
				{
					llvm::outs() << "Done " << variantsCount << " variants.\n";
				}

				llvm::outs() << "DEBUG: Processing valid bitmask " << Stringify(bitMask) << "\n";

				// TODO: Change the source file extension based on the programming language.
				auto fileName = TempFolder + std::to_string(variantsCount) + "_tempFile.c";
				printingConsumer_.HandleTranslationUnit(context, fileName, bitMask);
			}
		}

		llvm::outs() << "Done " << variantsCount << " variants.\n";
	}
};
#endif
