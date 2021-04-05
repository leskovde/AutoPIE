#pragma once
#include <clang/Basic/SourceLocation.h>
#include <clang/AST/Stmt.h>
#include <filesystem>
#include <clang/AST/ExternalASTSource.h>
#include <clang/Lex/Lexer.h>

class DependencyGraph;

using BitMask = std::vector<bool>;

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
	const std::string errorMessage;
	const Location errorLocation;
	const double reductionRatio;
	const bool dumpDot;

	InputData(const std::string& message, const Location& location, const double ratio, const bool dump) :
		errorMessage(message), errorLocation(location), reductionRatio(ratio), dumpDot(dump)
	{}
};

clang::SourceRange GetSourceRange(const clang::Stmt& s);

std::string RangeToString(clang::ASTContext& astContext, clang::SourceRange range);

int GetChildrenCount(clang::Stmt* st);

std::string ExecCommand(const char* cmd);

bool ClearTempDirectory(bool prompt);

std::string RemoveFileExtensions(const std::string& fileName);

std::string EscapeQuotes(const std::string& text);

bool IsFull(BitMask& bitMask);

void Increment(BitMask& bitMask);

bool IsValid(BitMask& bitMask, DependencyGraph& dependencies);

clang::SourceRange GetPrintableRange(clang::SourceRange range, const clang::SourceManager& sm);

std::string RangeToString(clang::ASTContext& astContext, clang::SourceRange range);

llvm::StringRef GetSourceTextRaw(clang::SourceRange range, const clang::SourceManager& sm);

llvm::StringRef GetSourceText(clang::SourceRange range, const clang::SourceManager& sm);
