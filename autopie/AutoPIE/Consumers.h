#pragma once
#include <clang/AST/ASTConsumer.h>

#include "Visitors.h"
#include <bitset>
#include "DependencyGraph.h"

class StatementReductionASTConsumer final : public clang::ASTConsumer
{
	StatementReductionASTVisitorRef visitor;

public:
	StatementReductionASTConsumer(clang::CompilerInstance* ci, GlobalContext& context)
		: visitor(std::make_unique<StatementReductionASTVisitor>(ci, context))
	{}

	void HandleTranslationUnit(clang::ASTContext& context) override
	{
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

	int GetCodeUnitsCount() const
	{
		return codeUnitsCount_;
	}

	DependencyGraph GetDependencyGraph()
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

		auto bitMask = std::vector<bool>(numberOfCodeUnits);
		auto dependencies = mappingConsumer_.GetDependencyGraph();

		while (!IsFull(bitMask))
		{
			Increment(bitMask);

			if (IsValid(bitMask, dependencies))
			{
				
			}
		}
	}
};