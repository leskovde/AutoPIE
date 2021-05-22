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

		bool IsMain(clang::Decl* decl) const
		{
			return llvm::isa<clang::FunctionDecl>(decl) && llvm::cast<clang::FunctionDecl>(decl)->isMain();
		}

		bool IsInSlice(const int startingLine, const int endingLine) const
		{
			return std::find(originalLines.begin(), originalLines.end(), startingLine) != originalLines.end() ||
				std::find(originalLines.begin(), originalLines.end(), endingLine) != originalLines.end();
		}

		bool HasSlicePartsInsideItsBody(const int bodyStartingLine, const int bodyEndingLine) const
		{
			for (auto line : originalLines)
			{
				if (bodyStartingLine <= line && line <= bodyEndingLine)
				{
					return true;
				}
			}

			return false;
		}
		
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

			if (decl->hasBody())
			{
				const auto bodyRange = GetPrintableRange(GetPrintableRange(decl->getBody()->getSourceRange(), astContext_.getSourceManager()),
					astContext_.getSourceManager());

				const auto bodyStartingLine = astContext_.getSourceManager().getSpellingLineNumber(bodyRange.getBegin());
				const auto bodyEndingLine = astContext_.getSourceManager().getSpellingLineNumber(bodyRange.getEnd());

				if (IsMain(decl) || IsInSlice(startingLine, endingLine) || HasSlicePartsInsideItsBody(bodyStartingLine, bodyEndingLine))
				{
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
			}
			else
			{
				if (IsMain(decl) || IsInSlice(startingLine, endingLine))
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
			
			if (IsInSlice(startingLine, endingLine))
			{
				for (auto i = startingLine; i <= endingLine; i++)
				{
					collectedLines.push_back(i);
				}

				// Const declarations might be absent in the slice even though they are referenced.
				// These declarations need to be added manually.
				if (llvm::isa<clang::DeclRefExpr>(stmt))
				{
					const auto decl = llvm::cast<clang::DeclRefExpr>(stmt)->getFoundDecl();

					if (!decl->hasBody())
					{
						const auto declRange = GetPrintableRange(GetPrintableRange(decl->getSourceRange(), astContext_.getSourceManager()),
							astContext_.getSourceManager());

						const auto declStartingLine = astContext_.getSourceManager().getSpellingLineNumber(declRange.getBegin());
						const auto declEndingLine = astContext_.getSourceManager().getSpellingLineNumber(declRange.getEnd());

						for (auto i = declStartingLine; i <= declEndingLine; i++)
						{
							collectedLines.push_back(i);
						}
					}
				}
			}
			
			return true;
		}
	};
}

#endif