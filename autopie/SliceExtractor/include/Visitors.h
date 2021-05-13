#ifndef VISITORS_SLICE_H
#define VISITORS_SLICE_H
#pragma once

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>

namespace SliceExtractor
{
	class SliceExtractorASTVisitor;
	using SliceExtractorASTVisitorRef = std::unique_ptr<SliceExtractorASTVisitor>;

	class SliceExtractorASTVisitor final : public clang::RecursiveASTVisitor<SliceExtractorASTVisitor>
	{
		clang::ASTContext& astContext_;

	public:

		std::vector<int>& originalLines;
		std::vector<int> collectedLines;

		explicit SliceExtractorASTVisitor(clang::CompilerInstance* ci, std::vector<int>& lines) : astContext_(ci->getASTContext()), originalLines(lines)
		{
		}

		bool VisitDecl(clang::Decl* decl)
		{
			const auto range = GetPrintableRange(GetPrintableRange(decl->getSourceRange(), astContext_.getSourceManager()),
				astContext_.getSourceManager());

			const auto startingLine = astContext_.getSourceManager().getSpellingLineNumber(range.getBegin());
			const auto endingLine = astContext_.getSourceManager().getSpellingLineNumber(range.getEnd());

			if (llvm::isa<clang::FunctionDecl>(decl) && llvm::cast<clang::FunctionDecl>(decl)->isMain() ||
				std::find(originalLines.begin(), originalLines.end(), startingLine) != originalLines.end() ||
				std::find(originalLines.begin(), originalLines.end(), endingLine) != originalLines.end())
			{
				if (decl->hasBody())
				{
					const auto bodyRange = GetPrintableRange(GetPrintableRange(decl->getBody()->getSourceRange(), astContext_.getSourceManager()),
						astContext_.getSourceManager());

					const auto bodyStartingLine = astContext_.getSourceManager().getSpellingLineNumber(bodyRange.getBegin());
					const auto bodyEndingLine = astContext_.getSourceManager().getSpellingLineNumber(bodyRange.getEnd());

					// The declaration has a body. The whole body does not need to be in the collected line list.
					// We only include that what is not inside the body (the signature and everything before and after
					// the body's braces, include those braces).

					for (auto i = startingLine; i <= bodyStartingLine; i++)
					{
						collectedLines.push_back(i);
					}

					for (auto i = bodyEndingLine; i <= endingLine; i++)
					{
						collectedLines.push_back(i);
					}
				}
				else
				{
					// No body => include the whole declaration.

					for (auto i = startingLine; i <= endingLine; i++)
					{
						collectedLines.push_back(i);
					}
				}
			}
			
			return true;
		}

		bool VisitStmt(clang::Stmt* stmt)
		{
			const auto range = GetPrintableRange(GetPrintableRange(stmt->getSourceRange(), astContext_.getSourceManager()),
				astContext_.getSourceManager());

			const auto startingLine = astContext_.getSourceManager().getSpellingLineNumber(range.getBegin());
			const auto endingLine = astContext_.getSourceManager().getSpellingLineNumber(range.getEnd());
			
			if (std::find(originalLines.begin(), originalLines.end(), startingLine) != originalLines.end() ||
				std::find(originalLines.begin(), originalLines.end(), endingLine) != originalLines.end())
			{
				for (auto i = startingLine; i <= endingLine; i++)
				{
					collectedLines.push_back(i);
				}
			}
			
			return true;
		}
	};
}

#endif