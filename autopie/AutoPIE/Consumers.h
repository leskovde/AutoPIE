#pragma once
#include <clang/AST/ASTConsumer.h>

#include "Visitors.h"
#include "Context.h"

class StatementReductionASTConsumer final : public clang::ASTConsumer
{
	StatementReductionASTVisitor* visitor;

public:
	StatementReductionASTConsumer(clang::CompilerInstance* ci, GlobalContext* ctx)
		: visitor(new StatementReductionASTVisitor(ci, ctx)) // initialize the visitor
	{
	}

	void HandleTranslationUnit(clang::ASTContext& context) override
	{
		/* we can use ASTContext to get the TranslationUnitDecl, which is
			 a single Decl that collectively represents the entire source file */
		visitor->TraverseDecl(context.getTranslationUnitDecl());
	}
};

class CountASTConsumer final : public clang::ASTConsumer
{
	CountASTVisitor* visitor;

public:
	explicit CountASTConsumer(clang::CompilerInstance* ci, GlobalContext* ctx)
		: visitor(new CountASTVisitor(ci, ctx)) // initialize the visitor
	{
	}

	void HandleTranslationUnit(clang::ASTContext& context) override
	{
		/* we can use ASTContext to get the TranslationUnitDecl, which is
			 a single Decl that collectively represents the entire source file */
		visitor->TraverseDecl(context.getTranslationUnitDecl());
	}
};