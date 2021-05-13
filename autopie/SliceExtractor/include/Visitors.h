#ifndef VISITORS_SLICE_H
#define VISITORS_SLICE_H
#pragma once

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Rewrite/Core/Rewriter.h>

#include <utility>

namespace SliceExtractor
{
	class SliceExtractorASTVisitor;
	using SliceExtractorASTVisitorRef = std::unique_ptr<SliceExtractorASTVisitor>;

	class SliceExtractorASTVisitor final : public clang::RecursiveASTVisitor<VariantPrintingASTVisitor>
	{
		clang::ASTContext& astContext_;
		std::vector<int>& lines_;

	public:

		explicit SliceExtractorASTVisitor(clang::CompilerInstance* ci, std::vector<int>& lines) : astContext_(ci->getASTContext()), lines_(lines)
		{
		}

		bool VisitDecl(clang::Decl* decl)
		{

			return true;
		}

		bool VisitStmt(clang::Stmt* stmt)
		{

			return true;
		}
	};
}

#endif