#ifndef ACTIONS_NAIVE_H
#define ACTIONS_NAIVE_H
#pragma once

#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>

#include "../../Common/include/Consumers.h"
#include "../../Common/include/Context.h"
#include "Consumers.h"

using namespace Common;

namespace Naive
{
	std::unique_ptr<clang::tooling::FrontendActionFactory> VariantGeneratingFrontendActionFactory(
		GlobalContext& context);

	/**
	 * Specifies the frontend action for generating source file variants.\n
	 * Currently creates the unifying consumer.
	 */
	class VariantGeneratingAction final : public clang::ASTFrontendAction
	{
		GlobalContext& globalContext_;

	public:

		explicit VariantGeneratingAction(GlobalContext& context) : globalContext_(context)
		{
		}

		std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& ci, llvm::StringRef /*file*/)
		override
		{
			return std::unique_ptr<clang::ASTConsumer
			>(std::make_unique<VariantGeneratingConsumer>(&ci, globalContext_));
		}
	};
} // namespace Naive
#endif
