#ifndef VISITORS_SLICE_H
#define VISITORS_SLICE_H
#pragma once

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>

namespace SliceExtractor
{
	class SliceExtractorASTVisitor;
	using SliceExtractorASTVisitorRef = std::unique_ptr<SliceExtractorASTVisitor>;

	/**
	 * A visitor for extracting additional line numbers.\n
	 * Some statements stretch over several lines - for example, loops.\n
	 * The used slicers don't always get all locations correctly and we have to scan for additional lines.\n
	 */
	class SliceExtractorASTVisitor final : public clang::RecursiveASTVisitor<SliceExtractorASTVisitor>
	{
		clang::ASTContext& astContext_;

		/**
		 * Checks whether a given declaration node is the program's main function.
		 *
		 * @param decl The given AST Decl node to be checked.
		 * @return True if the node corresponds to `main` and its body, false otherwise.
		 */
		bool IsMain(clang::Decl* decl) const
		{
			return llvm::isa<clang::FunctionDecl>(decl) && llvm::cast<clang::FunctionDecl>(decl)->isMain();
		}

		/**
		 * Scans the lines provided by the slicer in order to figure out whether a two points of the code
		 * are in the original slice.
		 *
		 * @param startingLine The first value to be checked.
		 * @param endingLine The second value to be checked.
		 * @return True if either of the two points are in the given slice, false otherwise.
		 */
		[[nodiscard]] bool IsInSlice(const int startingLine, const int endingLine) const
		{
			return std::find(originalLines.begin(), originalLines.end(), startingLine) != originalLines.end() ||
				std::find(originalLines.begin(), originalLines.end(), endingLine) != originalLines.end();
		}

		/**
		 * Scans the lines provided by the slicer in order to figure out whether a range in the code
		 * is in the original slice.
		 *
		 * @param bodyStartingLine The first value of the range.
		 * @param bodyEndingLine The second value of the range.
		 * @return True if any line in the original slice is in the specified range, false otherwise.
		 */
		[[nodiscard]] bool HasSlicePartsInsideItsBody(const int bodyStartingLine, const int bodyEndingLine) const
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

		explicit SliceExtractorASTVisitor(clang::CompilerInstance* ci, std::vector<int>& lines) : astContext_(
			                                                                                          ci->
			                                                                                          getASTContext()),
		                                                                                          originalLines(lines)
		{
		}

		/**
		 * Processes a declaration node.\n
		 * Extracts its locations and if any part of the declaration corresponds to the slice,
		 * its line numbers are collected.
		 */
		bool VisitDecl(clang::Decl* decl)
		{
			// There are multiple ways of checking header files.
			// In this case, we ignore all but the supplied file.
			if (!astContext_.getSourceManager().isInMainFile(decl->getBeginLoc()))
			{
				return true;
			}

			const auto range = GetPrintableRange(
				GetPrintableRange(decl->getSourceRange(), astContext_.getSourceManager()),
				astContext_.getSourceManager());

			const auto startingLine = astContext_.getSourceManager().getSpellingLineNumber(range.getBegin());
			const auto endingLine = astContext_.getSourceManager().getSpellingLineNumber(range.getEnd());

			if (decl->hasBody() && decl->getBody() != nullptr)
			{
				const auto bodyRange = GetPrintableRange(
					GetPrintableRange(decl->getBody()->getSourceRange(), astContext_.getSourceManager()),
					astContext_.getSourceManager());

				const auto bodyStartingLine = astContext_
				                              .getSourceManager().getSpellingLineNumber(bodyRange.getBegin());
				const auto bodyEndingLine = astContext_.getSourceManager().getSpellingLineNumber(bodyRange.getEnd());

				if (IsMain(decl) || IsInSlice(startingLine, endingLine) || HasSlicePartsInsideItsBody(
					bodyStartingLine, bodyEndingLine))
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

		/**
		 * Processes statement nodes.\n
		 * Checks for their location and if the location corresponds to any line in the slice,
		 * it collects all of the statement's line numbers.\n
		 * Additionally, if the statement is in the slice and it is a declaration reference to
		 * a declaration not in the slice (const variable, etc.), the declaration is added to the slice.
		 */
		bool VisitStmt(clang::Stmt* stmt)
		{
			// There are multiple ways of checking header files.
			// In this case, we ignore all but the supplied file.
			if (!astContext_.getSourceManager().isInMainFile(stmt->getBeginLoc()))
			{
				return true;
			}

			const auto range = GetPrintableRange(
				GetPrintableRange(stmt->getSourceRange(), astContext_.getSourceManager()),
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
						const auto declRange = GetPrintableRange(
							GetPrintableRange(decl->getSourceRange(), astContext_.getSourceManager()),
							astContext_.getSourceManager());

						const auto declStartingLine = astContext_.getSourceManager().getSpellingLineNumber(
							declRange.getBegin());
						const auto declEndingLine = astContext_.getSourceManager().getSpellingLineNumber(
							declRange.getEnd());

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
} // namespace SliceExtractor

#endif
