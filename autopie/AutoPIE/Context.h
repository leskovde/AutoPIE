#ifndef CONTEXT_H
#define CONTEXT_H
#pragma once

#include "clang/Rewrite/Core/Rewriter.h"

#include "Helper.h"

class CountVisitorContext
{
	int totalStatementCount;

public:

	CountVisitorContext()
	{
		totalStatementCount = 0;
	}

	explicit CountVisitorContext(const int lineCount)
	{
		totalStatementCount = lineCount;
	}

	void Increment()
	{
		totalStatementCount++;
	}

	void ResetStatementCount()
	{
		totalStatementCount = 0;
	}

	[[nodiscard]] int GetTotalStatementCount() const
	{
		return totalStatementCount;
	}
};

class StatementReductionContext
{
	int targetStatementNumber;
	bool sourceChanged = false;

public:
	StatementReductionContext()
	{
		targetStatementNumber = 1;
	}

	explicit StatementReductionContext(const int target)
	{
		targetStatementNumber = target;
	}

	void ChangeSourceStatus()
	{
		sourceChanged = true;
	}

	[[nodiscard]] bool GetSourceStatus() const
	{
		return sourceChanged;
	}

	[[nodiscard]] int GetTargetStatementNumber() const
	{
		return targetStatementNumber;
	}
};

class GlobalContext
{
	GlobalContext(): parsedInput(InputData("", Location("", 0), 0.0, false))
	{
		llvm::errs() << "DEBUG: GlobalContext - New default constructor call.\n";
	}

public:

	// AST traversal properties.
	clang::Rewriter globalRewriter;
	CountVisitorContext countVisitorContext;
	StatementReductionContext statementReductionContext;

	// Variant generation properties.
	InputData parsedInput;

	GlobalContext(InputData& input, const std::string& /*source*/) : parsedInput(input)
	{
		globalRewriter = clang::Rewriter();
		countVisitorContext = CountVisitorContext();
		statementReductionContext = StatementReductionContext();

		llvm::errs() << "DEBUG: GlobalContext - New non-default constructor call.\n";
	}
};
#endif
