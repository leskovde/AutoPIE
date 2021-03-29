#pragma once
#include <clang/AST/ASTConsumer.h>

#include "Visitors.h"
#include "DependencyGraph.h"

class StatementReductionASTConsumer final : public clang::ASTConsumer
{
	StatementReductionASTVisitorRef visitor;

public:
	StatementReductionASTConsumer(clang::CompilerInstance* ci, GlobalContext& context)
		: visitor(std::make_unique<StatementReductionASTVisitor>(ci, context))
	{}

	void HandleTranslationUnit(clang::ASTContext& context, const std::string& fileName, const BitMask bitMask) const
	{
		visitor->SetBitMask(bitMask);
		visitor->SetOutputFile(fileName);
		visitor->TraverseDecl(context.getTranslationUnitDecl());
	}
};

class MappingASTConsumer final : public clang::ASTConsumer
{
	int codeUnitsCount_ = 0;
	DependencyGraph graph_;
	CountASTVisitorRef visitor_;

public:
	MappingASTConsumer(clang::CompilerInstance* ci, GlobalContext& context)
		: visitor_(std::make_unique<CountASTVisitor>(ci, context))
	{}

	void HandleTranslationUnit(clang::ASTContext& context) override
	{
		visitor_->TraverseDecl(context.getTranslationUnitDecl());
	}

	[[nodiscard]] int GetCodeUnitsCount() const
	{
		return codeUnitsCount_;
	}

	DependencyGraph GetDependencyGraph() const
	{
		return graph_;
	}
};

class VariantGenerationConsumer final : public clang::ASTConsumer
{
	MappingASTConsumer mappingConsumer_;
	StatementReductionASTConsumer reductionConsumer_;

public:
	VariantGenerationConsumer(clang::CompilerInstance* ci, GlobalContext& context)
		: mappingConsumer_(MappingASTConsumer(ci, context)), reductionConsumer_(ci, context)
	{}
	
	void HandleTranslationUnit(clang::ASTContext& context) override
	{
		mappingConsumer_.HandleTranslationUnit(context);
		const auto numberOfCodeUnits = mappingConsumer_.GetCodeUnitsCount();

		// Udelat bitfield velikosti N.
		// Zjistit si k nemu nejaka pravidla (ktere bity jsou nadrazene, ktere patri pod ne).
		// Podle pravidel generovat bitmasky.
		// Podle bitmasky projit visitorem AST a vypsat ho.

		auto bitMask = BitMask(numberOfCodeUnits);
		auto dependencies = mappingConsumer_.GetDependencyGraph();

		llvm::outs() << "Maximum expected variants: " << pow(2, numberOfCodeUnits) << "\n";
		
		auto variantsCount = 0;
		
		while (!IsFull(bitMask))
		{
			Increment(bitMask);

			if (IsValid(bitMask, dependencies))
			{
				if (variantsCount % 50 == 0)
				{
					llvm::outs() << "Done " << variantsCount << " variants.\n";
				}

				variantsCount++;
				
				auto fileName = std::to_string(variantsCount) + "_tempFile.cpp";
				reductionConsumer_.HandleTranslationUnit(context, fileName, bitMask);
			}
		}
	}
};