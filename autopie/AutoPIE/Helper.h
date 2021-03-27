#pragma once
#include <clang/Basic/SourceLocation.h>
#include <clang/AST/Stmt.h>
#include <filesystem>
#include <clang/AST/ExternalASTSource.h>

struct Location
{
	std::string fileName;
	int lineNumber;

	Location(const std::string& locFile, const int locLine) :
		fileName(locFile), lineNumber(locLine)
	{}
};

struct InputData
{
	std::string errorMessage;
	Location errorLocation;
	double reductionRatio;

	InputData(const std::string& message, const Location& location, const double ratio) :
		errorMessage(message), errorLocation(location), reductionRatio(ratio)
	{}
};

clang::SourceRange GetSourceRange(const clang::Stmt& s);

int GetChildrenCount(clang::Stmt* st);

std::string ExecCommand(const char* cmd);

bool ClearTempDirectory(bool prompt);