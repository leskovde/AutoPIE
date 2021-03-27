#pragma once
#include <clang/Rewrite/Core/Rewriter.h>
#include <stack>
#include <iostream>
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

	int GetTotalStatementCount() const
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
	
	bool GetSourceStatus() const
	{
		return sourceChanged;
	}

	int GetTargetStatementNumber() const
	{
		return targetStatementNumber;
	}
};

class GlobalContext
{
	// TODO: Implement the rule of 5.
	// TODO: Change the raw pointer to an unique_ptr.
	static GlobalContext* instance;

	GlobalContext(InputData& input, const std::string& source) : parsedInput(input)
	{
		globalRewriter = clang::Rewriter();
		countVisitorContext = CountVisitorContext();
		statementReductionContext = StatementReductionContext();

		searchStack = std::stack<std::string>();
		searchStack.push(source);

		llvm::errs() << "DEBUG: GlobalContext - New non-default constructor call.\n";
	}

	GlobalContext(): parsedInput(InputData("", Location("", 0), 0.0))
	{
		llvm::errs() << "DEBUG: GlobalContext - New default constructor call.\n";
	}

public:

	// AST traversal properties.
	clang::Rewriter globalRewriter;
	CountVisitorContext countVisitorContext;
	StatementReductionContext statementReductionContext;

	// Variant generation properties.
	int iteration = 0;
	InputData parsedInput;
	std::stack<std::string> searchStack;

	GlobalContext(GlobalContext& other) = delete;

	void operator=(const GlobalContext&) = delete;
	
	static GlobalContext* GetInstance()
	{
		if (!instance)
		{
			instance = new GlobalContext();
		}
		
		return instance;
	}

	static GlobalContext* GetInstance(InputData& input, const std::string& source)
	{
		delete instance;

		instance = new GlobalContext(input, source);
		
		return instance;
	}
};