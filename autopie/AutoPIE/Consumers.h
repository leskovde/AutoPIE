#pragma once
#include <clang/AST/ASTConsumer.h>

#include "Visitors.h"
#include "DependencyGraph.h"

class VariantPrintingASTConsumer final : public clang::ASTConsumer
{
	VariantPrintingASTVisitorRef visitor_;

public:
	VariantPrintingASTConsumer(clang::CompilerInstance* ci, GlobalContext& context)
		: visitor_(std::make_unique<VariantPrintingASTVisitor>(ci, context))
	{}

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

	void SetData(SkippedMapRef skippedNodes, DependencyGraph graph)
	{
		visitor_->SetData(skippedNodes, graph);
	}
};

class DependencyMappingASTConsumer final : public clang::ASTConsumer
{
	NodeMappingRef nodeMapping_;
	MappingASTVisitorRef mappingVisitor_;
	GlobalContext& globalContext_;

public:
	DependencyMappingASTConsumer(clang::CompilerInstance* ci, GlobalContext& context) : globalContext_(context)
	{
		nodeMapping_ = std::make_shared<NodeMapping>();
		mappingVisitor_ = std::make_unique<MappingASTVisitor>(ci, nodeMapping_, globalContext_.parsedInput.errorLocation.lineNumber);
	}

	void HandleTranslationUnit(clang::ASTContext& context) override
	{
		// TODO: Handle specific node types instead of just Stmt, Decl and Expr.
		
		mappingVisitor_->TraverseDecl(context.getTranslationUnitDecl());

		llvm::outs() << "DEBUG: AST nodes counted: " << mappingVisitor_->codeUnitsCount << ", AST nodes actual: " << nodeMapping_->size() << "\n";

		mappingVisitor_->graph.PrintGraphForDebugging();

		if (globalContext_.parsedInput.dumpDot)
		{
			mappingVisitor_->graph.DumpDot(globalContext_.parsedInput.errorLocation.fileName);
		}
	}

	[[nodiscard]] int GetCodeUnitsCount() const
	{
		return nodeMapping_->size();
	}

	DependencyGraph GetDependencyGraph() const
	{
		return mappingVisitor_->graph;
	}

	SkippedMapRef GetSkippedNodes() const
	{
		return mappingVisitor_->GetSkippedNodes();
	}
};

class VariantGenerationConsumer final : public clang::ASTConsumer
{
	DependencyMappingASTConsumer mappingConsumer_;
	VariantPrintingASTConsumer printingConsumer_;

public:
	VariantGenerationConsumer(clang::CompilerInstance* ci, GlobalContext& context) :
		mappingConsumer_(ci, context), printingConsumer_(ci, context)
	{ }
	
	void HandleTranslationUnit(clang::ASTContext& context) override
	{
		mappingConsumer_.HandleTranslationUnit(context);
		const auto numberOfCodeUnits = mappingConsumer_.GetCodeUnitsCount();

		printingConsumer_.SetData(mappingConsumer_.GetSkippedNodes(), mappingConsumer_.GetDependencyGraph());

		auto bitMask = BitMask(numberOfCodeUnits);
		auto dependencies = mappingConsumer_.GetDependencyGraph();

		llvm::outs() << "Maximum expected variants: " << pow(2, numberOfCodeUnits) << "\n";

		//const auto concreteTestCase = BitMask{ true, true, true,  true, false, false, false, false, true, true, true};
		//bitMask = concreteTestCase;
		
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
				
				auto fileName = "temp/" + std::to_string(variantsCount) + "_tempFile.cpp";
				printingConsumer_.HandleTranslationUnit(context, fileName, bitMask);
			}
		}

		llvm::outs() << "Done " << variantsCount << " variants.\n";
	}
};