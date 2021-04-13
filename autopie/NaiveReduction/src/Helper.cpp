//===----------------------------------------------------------------------===//
//
/// Defines functions that would otherwise be static.
/// Contributes to smaller .cpp files.
//
//===----------------------------------------------------------------------===//

#include <clang/AST/ASTContext.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Driver/Compilation.h>
#include <clang/Driver/Driver.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>

#include <llvm/ADT/SmallVector.h>
#include <llvm/Object/MachO.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/VirtualFileSystem.h>

#include <filesystem>

#include "../include/DependencyGraph.h"
#include "../include/Helper.h"

//===----------------------------------------------------------------------===//
//
/// Source range helper functions.
//
//===----------------------------------------------------------------------===//

//TODO: Rework range <---> source functions to work with StringRef instead of std::string.

/**
 * Extracts the underlying source code from source range.\n
 * Uses other helper methods to extract text accurately.\n
 * Expect the source range to be in its default form, i.e., returned by the `node->getSourceRange()` method.
 *
 * @param astContext The AST context used to get a `SourceManager` instance.
 * @param range The source range from which the code should be extracted.
 * @return The corresponding source code as a std::string.
 */
std::string RangeToString(clang::ASTContext& astContext, const clang::SourceRange range)
{
	return GetSourceText(range, astContext.getSourceManager()).str();
}

/**
 * Corrects the given source range to a one that include all sensible tokens.\n
 * Uses lexer information for more accurate end location.
 *
 * @param range The source range given by the `node->getSourceRange()` method.
 * @param sm Source manager given by the AST context.
 * @return The corrected source range.
 */
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
 *
 * @param range The (perhaps corrected) source range to be processed.
 * @param sm Source manager from the AST context.
 * @return A reference to the string corresponding to the source range returned from lexer.
 */
llvm::StringRef GetSourceTextRaw(const clang::SourceRange range, const clang::SourceManager& sm)
{
	return clang::Lexer::getSourceText(clang::CharSourceRange::getCharRange(range), sm, clang::LangOptions());
}

/**
 * Gets the portion of the code that corresponds to given SourceRange, including the
 * last token. Returns expanded macros.
 *
 * @param range The (yet to be corrected) source range to be processed.
 * @param sm Source manager from the AST context.
 * @return A reference to the string corresponding to the corrected source range.
 */
llvm::StringRef GetSourceText(const clang::SourceRange range, const clang::SourceManager& sm)
{
	const auto printableRange = GetPrintableRange(GetPrintableRange(range, sm), sm);
	return GetSourceTextRaw(printableRange, sm);
}

//===----------------------------------------------------------------------===//
//
/// File, path, and directory helper functions.
//
//===----------------------------------------------------------------------===//

/**
 * Clears the default temporary directory.\n
 * (If prompted and answered positively,) removes all files inside the temp directory and recreates the directory.
 *
 * @param prompt Specifies whether the user should be prompted to confirm the directory deletion.
 */
bool ClearTempDirectory(const bool prompt)
{
	if (prompt && std::filesystem::exists(TempFolder))
	{
		out::All() << "WARNING: The path " << TempFolder << " exists and is about to be cleared! Do you want to proceed? [Y/n] ";
		const auto decision = std::getchar();
		out::All() << "\n";

		if (decision == 'n' || decision == 'N')
		{
			return false;
		}
	}

	out::All() << "Clearing the " << TempFolder << " directory...\n";

	std::filesystem::remove_all(TempFolder);
	std::filesystem::create_directory(TempFolder);

	return true;
}

/**
 * Removes the extension from a path, i.e., the last found substring starting with a dot.
 *
 * @param filePath The path in a string form.
 * @return The path stripped of the extension substring.
 */
std::string RemoveFileExtensions(const std::string& filePath)
{
	return filePath.substr(0, filePath.find_last_of('.'));
}

/**
 * Adds an additional backslash character to each double-quote character.
 *
 * @param text The string in which double-quotes should be escaped.
 * @return The modified string with additional backslashes.
 */
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

//===----------------------------------------------------------------------===//
//
/// BitMask helper functions.
//
//===----------------------------------------------------------------------===//

/**
 * Converts the bit mask container to a string of zeroes and ones.\n
 * Serves mainly for debugging and logging purposes.
 *
 * @param bitMask The bitmask to be converted to string.
 * @return The string form of the bitmask, read from the most significant bit.
 */
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

/**
 * Determines whether the given bitmask is full of ones.
 *
 * @param bitMask The bitmask to be traversed.
 * @return True if the bitmask contains only positive bits, false otherwise.
 */
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

/**
 * Adds a single bit to the given bitmask, performing a binary addition.\n
 * Carry-over bits are then incremented to the next more significant bit.\n
 * Upon overflow, the bitmask is set to all zeroes.
 *
 * @param bitMask A reference to the bitmask to be incremented to.
 */
void Increment(BitMask& bitMask)
{
	// TODO(Denis): Write unit tests for this function (and the all variant generation), e.g. bitmask overflow.

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

/**
 * Determines whether the bitmask that represents a certain source file variant is valid.\n
 * In order to be valid, it must satisfy the relationships given by the dependency graph.\n
 * If a parent code unit is set zero, so must be its children.\n
 * Code units on the error-inducing line must be present.
 *
 * @param bitMask The variant represent by a bitmask.
 * @param dependencies The code unit relationship graph.
 * @return True if the bitmask results in a valid source file variant in terms of code unit
 * relationships.
 */
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

//===----------------------------------------------------------------------===//
//
/// Variant validation helper functions.
//
//===----------------------------------------------------------------------===//

/**
 * Attempts to compile a given source file entry.\n
 * The compilation is done using clang, the source is being compiled to an executable using
 * options that should guarantee debug symbols present in the output.\n
 * The name of the output should correspond to the name of the source file. Its extension is
 * replaced with `.exe`.\n
 * The compilation is considered as a failed one if the compiler returns a non-zero exit code
 * or if the output file was not created.
 *
 * @param entry The file system entry for a source code file.
 * @param language The programming language in which the source file is written.
 * @return Zero if the code was successfully compiled, the compiler's different exit code otherwise.
 */
int Compile(const std::filesystem::directory_entry& entry, const clang::Language language)
{
	// TODO: Change arguments and clangPath based on the input language.
	
	const auto input = entry.path().string();
	const auto output = TempFolder + entry.path().filename().replace_extension(".exe").string();
	auto clangPath = llvm::sys::findProgramByName("clang");

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

/**
 * Check whether a given location specified by a file and a line number exists.\n
 * In case it does, the function prints a context containing of a set number of lines
 * before and after the given location.
 *
 * @param filePath The file to be checked.
 * @param lineNumber The line in the given file to be checked, numbering starts from 1.
 * @return True if the given file and line combination is accessible, false otherwise.
 */
bool CheckLocationValidity(const std::string& filePath, const long lineNumber)
{
	// TODO: Add unit tests for this function (out of bounds testing, locked file, etc.)
	
	std::ifstream ifs(filePath);

	if (!ifs)
	{
		return false;
	}
	
	// Read all content and split it into lines.

	auto ss = std::stringstream(std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()));
	auto lines = std::vector<std::string>();

	for (std::string line; std::getline(ss, line);)
	{
		lines.emplace_back(line);
	}

	if (lineNumber > lines.size())
	{
		return false;
	}

	// Print the context of the error-inducing line.

	const auto contextSize = 3;
	const auto contextStart = lineNumber - contextSize > 0 ? lineNumber - contextSize : 1;
	const auto contextEnd = lineNumber + contextSize < lines.size() ? lineNumber + contextSize : lines.size();

	out::All() << "===---------------- Context of the error-inducing line ------------------===\n";
	
	for (auto i = contextStart; i <= contextEnd; i++)
	{
		out::All() << (i != lineNumber ? "    " : "[*] ") << i << ": " << lines[i - 1] << "\n";
	}

	out::All() << "===----------------------------------------------------------------------===\n";

	return true;
}

/**
 * Converts the LLDB's StateType enum to a string message.
 *
 * @param state The state to be converted.
 * @return The message based on the matched state type. 'unknown' if the type was not recognized.
 */
std::string StateToString(const lldb::StateType state)
{
	switch (state)
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
		return "Unknown";
	}
}

/**
 * Converts the LLDB's StopReason enum to a string message.
 *
 * @param reason The reason to be converted.
 * @return The message based on the matched stop reason. 'unknown' if the reason was not recognized.
 */
std::string StopReasonToString(const lldb::StopReason reason)
{
	switch (reason)
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
		return "Unknown";
	}
}

std::string LanguageToString(const clang::Language lang)
{
	switch (lang)
	{
	case clang::Language::Asm:
		return "Assembly";
	case clang::Language::C:
		return "C";
	case clang::Language::CUDA:
		return "CUDA";
	case clang::Language::CXX:
		return "C++";
	case clang::Language::HIP:
		return "HIP";
	case clang::Language::LLVM_IR:
		return "LLVM IR";
	case clang::Language::ObjC:
		return "Objective-C";
	case clang::Language::ObjCXX:
		return "Objective-C++";
	case clang::Language::OpenCL:
		return "OpenCL";
	case clang::Language::RenderScript:
		return "RenderScript";
	case clang::Language::Unknown:
	default:
		return "Unknown";
	}
}
