#ifndef CONSUMERS_SLICE_H
#define CONSUMERS_SLICE_H
#pragma once

#include <clang/AST/ASTConsumer.h>

#include <algorithm>

#include "Visitors.h"

namespace SliceExtractor
{
	class SliceExtractorASTConsumer final : public clang::ASTConsumer
	{
		SliceExtractorASTVisitorRef sliceVisitor_{};

	public:
		SliceExtractorASTConsumer(clang::CompilerInstance* ci, std::vector<int>& lines)
		{
			sliceVisitor_ = std::make_unique<SliceExtractorASTVisitor>(ci, lines);
		}

		void HandleTranslationUnit(clang::ASTContext& context) override
		{
			sliceVisitor_->TraverseDecl(context.getTranslationUnitDecl());

			// Concatenate found lines.
			auto& originalLines = sliceVisitor_->originalLines;
			auto& collectedLines = sliceVisitor_->collectedLines;
			
			originalLines.insert(originalLines.end(), collectedLines.begin(), collectedLines.end());

			// Remove duplicates.
			std::sort(originalLines.begin(), originalLines.end());
			const auto it = std::unique(originalLines.begin(), originalLines.end());
			originalLines.erase(it, originalLines.end());
			
		}
	};
}

#endif