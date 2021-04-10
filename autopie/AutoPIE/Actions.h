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

	void EndSourceFileAction() override
	{
		/*
		if (!globalContext->statementReductionContext.GetSourceStatus())
		{
			return;
		}

		auto& sm = globalContext->globalRewriter.getSourceMgr();

		llvm::outs() << "\t\tEXPR REDUCTION ACTION: END OF FILE ACTION:\n";
		globalContext->globalRewriter.getEditBuffer(sm.getMainFileID()).write(llvm::errs());

		const auto fileName = "temp/" + std::to_string(globalContext->iteration) + TempName;

		std::error_code errorCode;
		llvm::raw_fd_ostream outFile(fileName, errorCode, llvm::sys::fs::F_None);

		globalContext->globalRewriter.getEditBuffer(sm.getMainFileID()).write(outFile);
		outFile.close();

		globalContext->searchStack.push(fileName);
		*/
	}

	std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& ci, llvm::StringRef /*file*/)
	override
	{
		return std::unique_ptr<clang::ASTConsumer>(std::make_unique<VariantGenerationConsumer>(&ci, globalContext));
	}
};
#endif
