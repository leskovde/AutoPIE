#pragma once
#include <filesystem>
#include <clang/Frontend/FrontendAction.h>

#include "Consumers.h"
#include "Context.h"

#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS

std::string TempName = "tempFile.cpp";

class StatementReduceAction final : public clang::ASTFrontendAction
{
	GlobalContext* globalContext = GlobalContext::GetInstance();

public:

	// Prints the updated source file to a new file specific to the current iteration.
	void EndSourceFileAction() override
	{
		if (!globalContext->statementReductionContext.GetSourceStatus())
		{
			return;
		}

		auto& sm = globalContext->globalRewriter.getSourceMgr();

		llvm::outs() << "\t\STMT REDUCTION ACTION: END OF FILE ACTION:\n";
		globalContext->globalRewriter.getEditBuffer(sm.getMainFileID()).write(llvm::errs());

		const auto fileName = "temp/" + std::to_string(globalContext->iteration) + TempName;

		std::error_code errorCode;
		llvm::raw_fd_ostream outFile(fileName, errorCode, llvm::sys::fs::F_None);

		globalContext->lastGeneratedFileName = fileName;
		
		globalContext->globalRewriter.getEditBuffer(sm.getMainFileID()).write(outFile); // --> this will write the result to outFile
		outFile.close();
	}

	std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& ci, llvm::StringRef file) override
	{
		return std::unique_ptr<clang::ASTConsumer>(std::make_unique<StatementReductionASTConsumer>(&ci, globalContext));
		// pass CI pointer to ASTConsumer
	}
};

class CountAction final : public clang::ASTFrontendAction
{
	GlobalContext* globalContext = GlobalContext::GetInstance();

public:

	// Prints the number of statements in the source code to the console
	void EndSourceFileAction() override
	{
		llvm::outs() << "\t\tCOUNT ACTION: END OF FILE ACTION:\n";
		llvm::outs() << "\t\t\tStatement count: " << globalContext->countVisitorContext.GetTotalStatementCount();
		llvm::outs() << "\n\n";
	}

	std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(clang::CompilerInstance& ci, llvm::StringRef file) override
	{
		return std::unique_ptr<clang::ASTConsumer>(std::make_unique<CountASTConsumer>(&ci, globalContext));
		// pass CI pointer to ASTConsumer
	}
};