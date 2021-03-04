#pragma once
#include <clang/Rewrite/Core/Rewriter.h>
#include <stack>
#include <iostream>

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
	static GlobalContext* instance;

	GlobalContext(const std::string& file, const int errorLine, const std::string& errorMessage)
	{
		countVisitorContext = CountVisitorContext();
		statementReductionContext = StatementReductionContext();

		globalRewriter = clang::Rewriter();
		variants = std::stack<std::string>();

		errorLineNumber = errorLine;
		errorLineMessage = errorMessage;
		fileName = file;

		variants.push(fileName);

		std::cout << "New non-default constructor call.\n";
	}

	GlobalContext() : errorLineNumber(0)
	{
		std::cout << "New default constructor call.\n";
	}
	
public:
	
	CountVisitorContext countVisitorContext;
	StatementReductionContext statementReductionContext;

	clang::Rewriter globalRewriter;
	std::stack<std::string> variants;

	int errorLineNumber;
	std::string errorLineMessage;
	std::string fileName;

	int iteration = 0;

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

	static GlobalContext* GetInstance(const std::string& file, const int errorLine, const std::string& errorMessage)
	{
		instance = new GlobalContext(file, errorLine, errorMessage);
		
		return instance;
	}
};