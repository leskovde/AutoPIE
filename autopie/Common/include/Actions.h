#ifndef ACTIONS_H
#define ACTIONS_H
#pragma once

#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>

#include "Consumers.h"
#include "Context.h"

std::unique_ptr<clang::tooling::FrontendActionFactory> VariantGeneratingFrontendActionFactory(GlobalContext& context);

/**
 * Specifies the frontend action for generating source file variants.\n
 * Currently creates a unifying consumer.
 */
class VariantGeneratingAction final : public clang::ASTFrontendAction
{
	GlobalContext& globalContext;

public:

	explicit VariantGeneratingAction(GlobalContext& context): globalContext(context)
	{
	}

	std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& ci, llvm::StringRef /*file*/)
	override
	{
		return std::unique_ptr<clang::ASTConsumer>(std::make_unique<VariantGeneratingConsumer>(&ci, globalContext));
	}
};
#endif
