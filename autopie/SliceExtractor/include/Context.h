#pragma once
#include <clang/Rewrite/Core/Rewriter.h>
#include <stack>
#include <iostream>
#include <vector>

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

class GlobalContext
{
	static GlobalContext* instance;

	explicit GlobalContext(const std::string& file)
	{
		lines = std::vector<int>();
		
		countVisitorContext = CountVisitorContext();

		globalRewriter = clang::Rewriter();

		currentFileName = file;

		std::cout << "New non-default constructor call.\n";
	}

	GlobalContext()
	{
		std::cout << "New default constructor call.\n";
	}

public:
	std::vector<int> lines;
	
	CountVisitorContext countVisitorContext;

	bool createNewRewriter{};
	clang::Rewriter globalRewriter;

	std::string currentFileName;
	std::string lastGeneratedFileName;

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

	static GlobalContext* GetInstance(const std::string& file)
	{
		instance = new GlobalContext(file);

		return instance;
	}
};