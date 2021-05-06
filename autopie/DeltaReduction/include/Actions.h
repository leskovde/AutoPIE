#ifndef ACTIONS_DELTA_H
#define ACTIONS_DELTA_H
#pragma once

#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>

#include "../../Common/include/Context.h"
#include "../../Common/include/Consumers.h"
#include "Consumers.h"

using namespace Common;

namespace Delta
{
	std::unique_ptr<clang::tooling::FrontendActionFactory> DeltaDebuggingFrontendActionFactory(GlobalContext& context);

	/**
	 * Specifies the frontend action for running the Delta debugging algorithm.\n
	 * Currently creates a unifying consumer.
	 */
	class DeltaDebuggingAction final : public clang::ASTFrontendAction
	{
		GlobalContext& globalContext;

	public:

		explicit DeltaDebuggingAction(GlobalContext& context) : globalContext(context)
		{
		}

		std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& ci, llvm::StringRef /*file*/)
			override
		{
			return std::unique_ptr<clang::ASTConsumer>(std::make_unique<DeltaDebuggingConsumer>(&ci, globalContext));
		}
	};
}

#endif