#ifndef ACTIONS_SLICE_H
#define ACTIONS_SLICE_H
#pragma once

#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>

#include "../../Common/include/Consumers.h"
#include "Consumers.h"

using namespace Common;

namespace SliceExtractor
{
	std::unique_ptr<clang::tooling::FrontendActionFactory> SliceExtractorFrontendActionFactory(std::vector<int>& lines);

	/**
	 * Specifies the frontend action for collecting relevant line numbers in a file.\n
	 * Currently creates the unifying consumer.
	 */
	class SliceExtractorAction final : public clang::ASTFrontendAction
	{
		std::vector<int>& lines_;

	public:

		explicit SliceExtractorAction(std::vector<int>& lines) : lines_(lines)
		{
		}

		std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& ci, llvm::StringRef /*file*/)
			override
		{
			return std::unique_ptr<clang::ASTConsumer>(std::make_unique<SliceExtractorASTConsumer>(&ci, lines_));
		}
	};
}

#endif