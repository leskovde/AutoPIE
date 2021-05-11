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
	std::unique_ptr<clang::tooling::FrontendActionFactory> DeltaDebuggingFrontendActionFactory(
		GlobalContext& context, int iteration, int partitionCount);

	/**
	 * Specifies the frontend action for running the Delta debugging algorithm.\n
	 * Currently creates a unifying consumer.
	 */
	class DeltaDebuggingAction final : public clang::ASTFrontendAction
	{
		int iteration_;
		int partitionCount_;
		GlobalContext& globalContext_;

	public:

		DeltaDebuggingAction(GlobalContext& context, const int iteration,
		                              const int partitionCount) : iteration_(iteration),
		                                                          partitionCount_(partitionCount),
		                                                          globalContext_(context)
		{
		}

		std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& ci, llvm::StringRef /*file*/)
			override
		{
			return std::unique_ptr<clang::ASTConsumer>(std::make_unique<DeltaDebuggingConsumer>(&ci, globalContext_, iteration_, partitionCount_));
		}
	};
}

#endif