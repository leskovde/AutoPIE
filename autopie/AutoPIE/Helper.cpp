#include <clang/Basic/SourceLocation.h>
#include <clang/AST/Stmt.h>
#include <stdexcept>

#include "Helper.h"
#include "DependencyGraph.h"
#include <clang/Basic/SourceManager.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/AST/ASTContext.h>

clang::SourceRange GetSourceRange(const clang::Stmt& s)
{
	return { s.getBeginLoc(), s.getEndLoc() };
}

std::string RangeToString(clang::ASTContext& astContext, const clang::SourceRange range)
{
	return GetSourceText(range, astContext.getSourceManager()).str();
}

int GetChildrenCount(clang::Stmt* st)
{
	int childrenCount = 0;

	for (auto it = st->children().begin(); it != st->children().end(); ++it)
	{
		childrenCount++;
	}

	for (auto child : st->children())
	{
		childrenCount += GetChildrenCount(child);
	}

	return childrenCount;
}

std::string ExecCommand(const char* cmd)
{
	std::array<char, 128> buffer;
	std::string result;
	const std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);

	if (!pipe)
	{
		throw std::runtime_error("PIPE ERROR: _popen() failed!");
	}

	while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
	{
		result += buffer.data();
	}

	return result;
}

bool ClearTempDirectory(const bool prompt = false)
{
	if (prompt && std::filesystem::exists("temp/"))
	{
		llvm::outs() << "WARNING: The path 'temp/' exists and is about to be cleared! Do you want to proceed? [Y/n] ";
		const auto decision = std::getchar();
		llvm::outs() << "\n";

		if (decision == 'n' || decision == 'N')
		{
			return false;
		}
	}

	llvm::outs() << "Clearing the 'temp/' directory...\n";

	std::filesystem::remove_all("temp/");
	std::filesystem::create_directory("temp");

	return true;
}

std::string Stringify(BitMask& bitMask)
{
	std::string bits;

	for (auto bit : bitMask)
	{
		if (bit)
		{
			bits.append("1");
		}
		else
		{
			bits.append("0");
		}
	}

	return bits;
}

bool IsFull(BitMask& bitMask)
{
	for (const auto& bit : bitMask)
	{
		if (!bit)
		{
			return false;
		}
	}

	return true;
}

void Increment(BitMask& bitMask)
{
	// TODO: Write unit tests for this function (and the all variant generation).
	
	for (size_t i = bitMask.size(); i > 0; i--)
	{
		if (bitMask[i - 1])
		{
			bitMask[i - 1].flip();
		}
		else
		{
			bitMask[i - 1].flip();
			break;
		}
	}
}

bool IsValid(BitMask& bitMask, DependencyGraph& dependencies)
{
	for (size_t i = 0; i < bitMask.size(); i++)
	{
		if (!bitMask[i])
		{
			if (dependencies.IsInCriterion(i))
			{
				// Criterion nodes should be present.
				return false;
			}
			
			auto children = dependencies.GetDependentNodes(i);

			for (auto child : children)
			{
				// Missing parent statement requires missing
				// children as well.
				if (bitMask[child])
				{
					return false;
				}
			}
		}
	}

	return true;
}

clang::SourceRange GetPrintableRange(const clang::SourceRange range, const clang::SourceManager& sm)
{
	const clang::LangOptions lo;

	const auto startLoc = sm.getSpellingLoc(range.getBegin());
	const auto lastTokenLoc = sm.getSpellingLoc(range.getEnd());
	const auto endLoc = clang::Lexer::getLocForEndOfToken(lastTokenLoc, 0, sm, lo);
	return clang::SourceRange{ startLoc, endLoc };
}

/**
 * Gets the portion of the code that corresponds to given SourceRange exactly as
 * the range is given.
 *
 * @warning The end location of the SourceRange returned by some Clang functions
 * (such as clang::Expr::getSourceRange) might actually point to the first character
 * (the "location") of the last token of the expression, rather than the character
 * past-the-end of the expression like clang::Lexer::getSourceText expects.
 * GetSourceTextRaw() does not take this into account. Use GetSourceText()
 * instead if you want to get the source text including the last token.
 *
 * @warning This function does not obtain the source of a macro/preprocessor expansion.
 * Use GetSourceText() for that.
 */
llvm::StringRef GetSourceTextRaw(const clang::SourceRange range, const clang::SourceManager& sm)
{
	return clang::Lexer::getSourceText(clang::CharSourceRange::getCharRange(range), sm, clang::LangOptions());
}


/**
 * Gets the portion of the code that corresponds to given SourceRange, including the
 * last token. Returns expanded macros.
 */
llvm::StringRef GetSourceText(const clang::SourceRange range, const clang::SourceManager& sm)
{
	const auto printableRange = GetPrintableRange(GetPrintableRange(range, sm), sm);
	return GetSourceTextRaw(printableRange, sm);
}

std::string RemoveFileExtensions(const std::string& fileName)
{
	return fileName.substr(0, fileName.find_last_of("."));
}

std::string EscapeQuotes(const std::string& text)
{
	auto result = std::string();

	for (auto character : text)
	{
		if (character == '\"')
		{
			result.push_back('\\');
		}

		result.push_back(character);
	}

	return result;
}