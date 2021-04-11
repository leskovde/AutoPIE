#ifndef ACTIONS_H
#define ACTIONS_H
#pragma once

#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>

#include "Consumers.h"
#include "Context.h"

std::unique_ptr<clang::tooling::FrontendActionFactory> CustomFrontendActionFactory(GlobalContext& context);

class VariantGenerationAction final : public clang::ASTFrontendAction
{
	GlobalContext& globalContext;

public:

	explicit VariantGenerationAction(GlobalContext& context): globalContext(context)
	{
	}

	std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& ci, llvm::StringRef /*file*/)
	override
	{
		return std::unique_ptr<clang::ASTConsumer>(std::make_unique<VariantGenerationConsumer>(&ci, globalContext));
	}
};
#endif
