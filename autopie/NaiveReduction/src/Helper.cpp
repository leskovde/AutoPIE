#include <clang/AST/ASTContext.h>
#include <clang/AST/Stmt.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>

#include <filesystem>

#include "../include/DependencyGraph.h"
#include "../include/Helper.h"
#include <llvm/Support/Program.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Driver/Driver.h>
#include <llvm/Support/Host.h>

clang::SourceRange GetSourceRange(const clang::Stmt& s)
{
	return {s.getBeginLoc(), s.getEndLoc()};
}

std::string RangeToString(clang::ASTContext& astContext, const clang::SourceRange range)
{
	return GetSourceText(range, astContext.getSourceManager()).str();
}

bool ClearTempDirectory(const bool prompt)
{
	if (prompt && std::filesystem::exists(TempFolder))
	{
		llvm::outs() << "WARNING: The path " << TempFolder << " exists and is about to be cleared! Do you want to proceed? [Y/n] ";
		const auto decision = std::getchar();
		llvm::outs() << "\n";

		if (decision == 'n' || decision == 'N')
		{
			return false;
		}
	}

	llvm::outs() << "Clearing the " << TempFolder << " directory...\n";

	std::filesystem::remove_all(TempFolder);
	std::filesystem::create_directory(TempFolder);

	return true;
}

std::string Stringify(BitMask& bitMask)
{
	std::string bits;

	for (const auto& bit : bitMask)
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
	// TODO(Denis): Write unit tests for this function (and the all variant generation).

	for (auto i = bitMask.size(); i > 0; i--)
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
	return clang::SourceRange{startLoc, endLoc};
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
	return fileName.substr(0, fileName.find_last_of('.'));
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

int Compile(const std::filesystem::directory_entry& entry)
{
	const auto input = entry.path().string();
	const auto output = TempFolder + entry.path().filename().replace_extension(".exe").string();
	auto clangPath = llvm::sys::findProgramByName("clang");

	std::filesystem::file_time_type lastWrite;

	const auto arguments = std::vector<const char*>{
		clangPath->c_str(), /*"-v",*/ "-O0", "-g", "-o", output.c_str(), input.c_str()
	};

	clang::DiagnosticOptions diagnosticOptions;
	const auto textDiagnosticPrinter = std::make_unique<clang::TextDiagnosticPrinter>(llvm::outs(), &diagnosticOptions);
	llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> diagIDs;

	auto diagnosticsEngine = std::make_unique<clang::DiagnosticsEngine>(diagIDs, &diagnosticOptions,
		textDiagnosticPrinter.get());

	clang::driver::Driver driver(arguments[0], llvm::sys::getDefaultTargetTriple(), *diagnosticsEngine.release());

	const auto compilation = std::unique_ptr<clang::driver::Compilation>(driver.BuildCompilation(arguments));

	//driver.PrintActions(*compilation);

	auto result = 1; // Set the initial value to invalid, since we don't need those imposter vibes in our lives.
	llvm::SmallVector<std::pair<int, const clang::driver::Command*>, 16> failingCommands;

	if (compilation)
	{
		result = driver.ExecuteCompilation(*compilation, failingCommands);
	}

	llvm::outs() << "\n";

	if (!std::filesystem::exists(output))
	{
		result |= 1;
	}

	return result;
}

std::string StateToString(const lldb::StateType st)
{
	switch (st)
	{
	case lldb::eStateInvalid:
		return "Invalid";
	case lldb::eStateUnloaded:
		return "Unloaded";
	case lldb::eStateConnected:
		return "Connected";
	case lldb::eStateAttaching:
		return "Attaching";
	case lldb::eStateLaunching:
		return "Launching";
	case lldb::eStateStopped:
		return "Stopped";
	case lldb::eStateRunning:
		return "Running";
	case lldb::eStateStepping:
		return "Stepping";
	case lldb::eStateCrashed:
		return "Crashed";
	case lldb::eStateDetached:
		return "Detached";
	case lldb::eStateExited:
		return "Exited";
	case lldb::eStateSuspended:
		return "Suspended";
	default:
		return "unknown";
	}
}

std::string StopReasonToString(const lldb::StopReason sr)
{
	switch (sr)
	{
	case lldb::eStopReasonInvalid:
		return "Invalid";
	case lldb::eStopReasonNone:
		return "None";
	case lldb::eStopReasonTrace:
		return "Trace";
	case lldb::eStopReasonBreakpoint:
		return "Breakpoint";
	case lldb::eStopReasonWatchpoint:
		return "Watchpoint";
	case lldb::eStopReasonSignal:
		return "Signal";
	case lldb::eStopReasonException:
		return "Exception";
	case lldb::eStopReasonExec:
		return "Exec";
	case lldb::eStopReasonPlanComplete:
		return "Plan Complete";
	case lldb::eStopReasonThreadExiting:
		return "Thread Exiting";
	case lldb::eStopReasonInstrumentation:
		return "Instrumentation";
	default:
		return "unknown";
	}
}
