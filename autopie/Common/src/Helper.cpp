#include <clang/AST/ASTContext.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/SourceLocation.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Driver/Compilation.h>
#include <clang/Driver/Driver.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>

#include <lldb/API/SBError.h>
#include <lldb/API/SBListener.h>
#include <lldb/API/SBProcess.h>
#include <lldb/API/SBStream.h>
#include <lldb/API/SBTarget.h>
#include <lldb/API/SBThread.h>

#include <llvm/ADT/SmallVector.h>
#include <llvm/Object/MachO.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/VirtualFileSystem.h>

#include <filesystem>
#include <optional>

#include "../include/Context.h"
#include "../include/DependencyGraph.h"
#include "../include/Helper.h"

//===----------------------------------------------------------------------===//
//
/// Source range helper functions.
//
//===----------------------------------------------------------------------===//

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
		Out::All() << "WARNING: The path " << TempFolder <<
			" exists and is about to be cleared! Do you want to proceed? [Y/n] ";
		const auto decision = std::getchar();
		Out::All() << "\n";

		if (decision == 'n' || decision == 'N')
		{
			return false;
		}
	}

	Out::All() << "Clearing the " << TempFolder << " directory...\n";

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
 * Removes all but the last element of a path. Furthermore, the extension is also removed.
 *
 * @param filePath The path in a string form.
 * @return The path stripped directories and the extension substring.
 */
std::string GetFileName(const std::string& filePath)
{
	return RemoveFileExtensions(filePath.substr(filePath.find_last_of("/\\") + 1));
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
std::string Stringify(const BitMask& bitMask)
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
 * Sets the given bit mask's bits to those representing the given number.\n
 * The initialization is done so that the last bit in the bit mask
 * (.size() -1) represents the least significant bit in the number.
 *
 * @param bitMask The bit mask to be modified.
 * @param number The unsigned integers whose bits will be copied.
 */
void InitializeBitMask(BitMask& bitMask, Unsigned number)
{
	for (auto i = bitMask.size(); number != 0; i--)
	{
		if (number & 1)
		{
			bitMask[i - 1] = true;
		}
		else
		{
			bitMask[i - 1] = false;
		}

		number >>= 1;
	}
}

/**
 * Merges two maps of bit mask containers.
 * Results are saved to the latter map.
 *
 * @param from The map from which data is taken.
 * @param to The map to which data is saved.
 */
void MergeVectorMaps(EpochRanges& from, EpochRanges& to)
{
	for (auto it = from.begin(); it != from.end(); ++it)
	{
		const auto toIt = to.insert(*it);

		if (!toIt.second)
		{
			auto fromElement = &it->second;
			auto toElement = &toIt.first->second;

			toElement->insert(toElement->end(), fromElement->begin(), fromElement->end());
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
 * @param heuristics Specifies whether a dependency graph related heuristics should be used to determine
 * the validity of the variant.
 * @return A pair of values. True if the bitmask results in a valid source file variant in terms of code unit
 * relationships. If valid, the second value is set to the variant's size ratio when compared to the original size.
 */
std::pair<bool, double> IsValid(const BitMask& bitMask, DependencyGraph& dependencies, const bool heuristics)
{
	auto characterCount = dependencies.GetTotalCharacterCount();

	for (size_t i = 0; i < bitMask.size(); i++)
	{
		if (!bitMask[i])
		{
			characterCount -= dependencies.GetNodeInfo(i).characterCount;

			if (dependencies.IsInCriterion(i))
			{
				// Criterion nodes should be present.
				return std::pair<bool, double>(false, 0);
			}

			if (heuristics)
			{
				for (auto child : dependencies.GetDependentNodes(i))
				{
					// The parent will be removed and there is no point in keeping its children.
					if (bitMask[child])
					{
						return std::pair<bool, double>(false, 0);
					}
				}
			}
		}
	}

	return std::pair<bool, double>(true, static_cast<double>(characterCount) / dependencies.GetTotalCharacterCount());
}

//===----------------------------------------------------------------------===//
//
/// Variant validation helper functions.
//
//===----------------------------------------------------------------------===//

/**
 * Determines the compiler based on the given language.\n
 *
 * @param language The language for which the compiler should be used.
 * @return A string with the name of compiler that can be used for finding it later.
 */
static std::string GetCompilerName(const clang::Language language)
{
	switch (language)
	{
	case clang::Language::C:
		return "clang";
	case clang::Language::CXX:
		return "clang++";
	default:
		throw std::invalid_argument("Language not supported.");
	}
}

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
	// Create the paths necessary for the compiler driver.
	const auto input = entry.path().string();
	const auto output = TempFolder + entry.path().filename().replace_extension(".out").string();
	const auto clangPath = llvm::sys::findProgramByName(GetCompilerName(language));

	// Compile using debug symbols - trivial arguments, a relaxation of the fully-fledged solution.
	const auto arguments = std::vector<const char*>{
		clangPath->c_str(), /*"-v",*/ "-O0", "-g", "-o", output.c_str(), input.c_str()
	};

	// Create the driver's components.
	clang::DiagnosticOptions diagnosticOptions;
	const auto textDiagnosticPrinter = std::make_unique<clang::TextDiagnosticPrinter>(llvm::outs(), &diagnosticOptions);
	llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs> diagIDs;

	auto diagnosticsEngine = std::make_unique<clang::DiagnosticsEngine>(diagIDs, &diagnosticOptions,
	                                                                    textDiagnosticPrinter.get());

	// Create the driver and assign the compilation to it.
	clang::driver::Driver driver(arguments[0], llvm::sys::getDefaultTargetTriple(), *diagnosticsEngine.release());

	const auto compilation = std::unique_ptr<clang::driver::Compilation>(driver.BuildCompilation(arguments));

	auto result = 1; // Set the initial value to invalid, since we don't need that lame energy in our lives.
	llvm::SmallVector<std::pair<int, const clang::driver::Command*>, 16> failingCommands;

	if (compilation)
	{
		// If valid, run the compilation.
		result = driver.ExecuteCompilation(*compilation, failingCommands);
	}

	llvm::outs() << "\n";

	// Determine the result based on whether the output binary exists.
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
 * @param force Specifies whether the output (the printing part) should be flushed to the All stream or not.
 * @return True if the given file and line combination is accessible, false otherwise.
 */
bool CheckLocationValidity(const std::string& filePath, const size_t lineNumber, const bool force)
{
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

	if (force)
	{
		Out::All() << "===---------------- Context of the error-inducing line ------------------===\n";
	}
	else
	{
		Out::Verb() << "===---------------- Context of the error-inducing line ------------------===\n";
	}

	for (auto i = contextStart; i <= contextEnd; i++)
	{
		if (force)
		{
			Out::All() << (i != lineNumber ? "    " : "[*] ") << i << ": " << lines[i - 1] << "\n";
		}
		else
		{
			Out::Verb() << (i != lineNumber ? "    " : "[*] ") << i << ": " << lines[i - 1] << "\n";
		}
	}

	if (force)
	{
		Out::All() << "===----------------------------------------------------------------------===\n";
	}
	else
	{
		Out::Verb() << "===----------------------------------------------------------------------===\n";
	}

	return true;
}

template <typename Out>
void SplitToWords(const std::string& s, const char delimiter, Out result)
{
	std::istringstream iss(s);
	std::string word;

	while (std::getline(iss, word, delimiter))
	{
		*result++ = word;
	}
}

/**
 * Separates a given string into a container strings based on a given delimiter.
 *
 * @param s The string to be split.
 * @param delimiter The character that is taken into consideration when splitting.
 */
std::vector<std::string> SplitToWords(const std::string& s, const char delimiter)
{
	std::vector<std::string> words;

	SplitToWords(s, delimiter, std::back_inserter(words));

	return words;
}

/**
 * Checks whether a given message roughly corresponds to the error message
 * specified in the program's options.\n
 * The validation is done by checking whether the current message contains
 * parts of the original. Any part suffices.
 *
 * @param currentMessage The message in which we search for parts of
 * the original.
 * @return True if at least a part of the original was found, false otherwise.
 */
static bool IsErrorMessageValid(const std::string& currentMessage)
{
	auto original = std::string(ErrorMessage);
	auto actual = std::string(currentMessage);

	// Convert both to the same case
	std::for_each(original.begin(), original.end(), [](char& c)
	{
		c = std::tolower(c);
	});

	std::for_each(actual.begin(), actual.end(), [](char& c)
	{
		c = std::tolower(c);
	});

	auto messageParts = SplitToWords(original, ' ');

	// Check the parts of the original message.
	for (auto& part : messageParts)
	{
		if (actual.find(part) != std::string::npos)
		{
			return true;
		}
	}

	return false;
}

/**
 * Runs the compiler and the LLDB debugger in order to validate a given source file.
 * If the compilation success, the LLDB proceeds to execute the generated binary
 * and checks how it executes.\n
 * Every time the execution stops, we check the location of the current symbol and
 * the generated message.\n
 * If both correspond, we terminate positively.
 * Otherwise, the search is inconclusive.
 * Note that the LLDB runtime has a set timeout that can only be changed inside the code.
 *
 * @param globalContext The algorithm's context used for extracting adjusted line numbers.
 * @param entry The filesystem's file entry.
 * @return True if the source code can be compiled and ends in the desired runtime error, false otherwise.
 */
bool ValidateVariant(GlobalContext& globalContext, const std::filesystem::directory_entry& entry)
{
	const auto compilationExitCode = Compile(entry, globalContext.language);

	if (compilationExitCode != 0)
	{
		// File could not be compiled, continue.
		return false;
	}

	const auto currentVariantName = entry.path().filename().string();
	const auto currentVariant = std::stol(currentVariantName.substr(0, currentVariantName.find('_')));

	const auto presumedErrorLines = globalContext.variantAdjustedErrorLocations[currentVariant];

	Out::Verb() << "Processing file: " << entry.path().string() << "\n";

	// Keep all LLDB logic written explicitly, not refactored in a function.
	// The function could be called when the LLDBSentry is not initialized => unwanted behaviour.
	// Having this function is a risk already...

	// Create a debugger object - represents an instance of LLDB.
	auto debugger(lldb::SBDebugger::Create());

	if (!debugger.IsValid())
	{
		llvm::errs() << "Debugger could not be created.\n";
		exit(EXIT_FAILURE);
	}

	const char* argv[] = {Arguments.c_str(), nullptr};

	lldb::SBError error;
	lldb::SBLaunchInfo launchInfo(argv);

	launchInfo.SetWorkingDirectory(TempFolder);
	launchInfo.SetLaunchFlags(lldb::eLaunchFlagExec | lldb::eLaunchFlagDebug);

	const auto executable = TempFolder + entry.path().filename().replace_extension(".out").string();
	Out::Verb() << "\nLLDB Target creation for " << executable << " ...\n";

	// Create and launch a target - represents a debugging session of a single executable.
	// Launching creates a new process in which the executable is ran.
	auto target = debugger.CreateTarget(executable.c_str(), llvm::sys::getDefaultTargetTriple().c_str(), "", true,
	                                    error);

	Out::Verb() << "error.Success()              = " << static_cast<int>(error.Success()) << "\n";
	Out::Verb() << "target.IsValid()             = " << static_cast<int>(target.IsValid()) << "\n";

	Out::Verb() << "\nLLDB Process launch ...\n";

	auto process = target.Launch(launchInfo, error);
	Out::Verb() << "error.Success()              = " << static_cast<int>(error.Success()) << "\n";
	Out::Verb() << "process.IsValid()            = " << static_cast<int>(process.IsValid()) << "\n";
	Out::Verb() << "process.GetProcessID()       = " << process.GetProcessID() << "\n";
	Out::Verb() << "process.GetState()           = " << StateToString(process.GetState()) << "\n";
	Out::Verb() << "process.GetNumThreads()      = " << process.GetNumThreads() << "\n";

	auto listener = debugger.GetListener();
	Out::Verb() << "listener.IsValid()           = " << static_cast<int>(listener.IsValid()) << "\n";

	auto done = false;
	// The timeout is currently set to 30 seconds for EACH event, not the entire run.
	const auto timeOut = 30;

	// The debugger is set to run asynchronously (debugger.GetAsync() => true).
	// The communication is done via events. Listen for events broadcast by the forked process.
	// Events are handled depending on the state of the process, the most important is `eStateStopped`
	// (stopped at a breakpoint, such as an exception) and `eStateExited` (ran without errors).
	while (!done)
	{
		lldb::SBEvent event;

		if (listener.WaitForEvent(timeOut /*seconds*/, event))
		{
			if (lldb::SBProcess::EventIsProcessEvent(event))
			{
				const auto state = lldb::SBProcess::GetStateFromEvent(event);

				if (state == lldb::eStateInvalid)
				{
					Out::Verb() << "Invalid process event: " << StateToString(state) << "\n";
				}
				else
				{
					Out::Verb() << "Process state event changed to: " << StateToString(state) << "\n";

					if (state == lldb::eStateStopped)
					{
						// The debugger stopped at a breakpoint. Since no breakpoints were set, this is most likely an exception.
						// Analyze the current stack frame to determine the status and position of the error.

						Out::Verb() << "Stopped at a breakpoint.\n";
						Out::Verb() << "LLDB Threading ...\n";

						auto thread = process.GetSelectedThread();
						Out::Verb() << "thread.IsValid()             = " << static_cast<int>(thread.IsValid()) << "\n";
						Out::Verb() << "thread.GetThreadID()         = " << thread.GetThreadID() << "\n";
						Out::Verb() << "thread.GetName()             = " << (thread.GetName() != nullptr
							                                                     ? thread.GetName()
							                                                     : "(null)") << "\n";
						Out::Verb() << "thread.GetStopReason()       = " << StopReasonToString(thread.GetStopReason())
							<<
							"\n";
						Out::Verb() << "thread.IsSuspended()         = " << static_cast<int>(thread.IsSuspended()) <<
							"\n";
						Out::Verb() << "thread.IsStopped()           = " << static_cast<int>(thread.IsStopped()) <<
							"\n";
						Out::Verb() << "process.GetState()           = " << StateToString(process.GetState()) << "\n";

						if (thread.GetStopReason() == lldb::StopReason::eStopReasonException)
						{
							Out::Verb() << "An exception was hit, killing the process ...\n";
							done = true;
						}

						auto frame = thread.GetSelectedFrame();
						Out::Verb() << "frame.IsValid()              = " << static_cast<int>(frame.IsValid()) << "\n";

						auto function = frame.GetFunction();

						if (function.IsValid())
						{
							Out::Verb() << "function.GetDisplayName()   = " << (function.GetDisplayName() != nullptr
								                                                    ? function.GetDisplayName()
								                                                    : "(null)") << "\n";
						}

						auto symbol = frame.GetSymbol();
						Out::Verb() << "symbol.IsValid()             = " << static_cast<int>(symbol.IsValid()) << "\n";

						if (symbol.IsValid())
						{
							Out::Verb() << "symbol.GetDisplayName()      = " << symbol.GetDisplayName() << "\n";

							auto symbolContext = frame.GetSymbolContext(lldb::eSymbolContextLineEntry);

							Out::Verb() << "symbolContext.IsValid()      = " << static_cast<int>(symbolContext.IsValid()
								)
								<< "\n";

							if (symbolContext.IsValid())
							{
								const auto fileName = symbolContext.GetLineEntry().GetFileSpec().GetFilename();
								const auto lineNumber = symbolContext.GetLineEntry().GetLine();

								Out::Verb() << "symbolContext.GetFilename()  = " << fileName << "\n";
								Out::Verb() << "symbolContext.GetLine()      = " << lineNumber << "\n";
								Out::Verb() << "symbolContext.GetColumn()    = " << symbolContext
								                                                    .GetLineEntry().GetColumn() << "\n";

								// Iterate over all possible error-inducing lines (the original, plus function decl body)
								// and determine whether the current location is correct.
								for (auto presumedErrorLine : presumedErrorLines)
								{
									if (lineNumber == presumedErrorLine)
									{
										auto stream = lldb::SBStream();
										thread.GetStatus(stream);

										Out::Verb() << "stream.IsValid()              = " << static_cast<int>(stream.
											IsValid()) << "\n";

										// The location was correct, validate the message - the runtime error must
										// be the same.
										if (stream.IsValid())
										{
											const auto currentMessage = stream.GetData();

											Out::Verb() << "stream.GetData()              = " << currentMessage << "\n";

											if (IsErrorMessageValid(currentMessage))
											{
												return true;
											}
										}

										break;
									}
								}
							}
						}

						process.Continue();
					}
					else if (state == lldb::eStateExited)
					{
						Out::Verb() << "Process exited.\n";

						auto description = process.GetExitDescription();

						if (description != nullptr)
						{
							Out::Verb() << "Exit status " << process.GetExitStatus() << ":" << description << "\n";
						}
						else
						{
							Out::Verb() << "Exit status " << process.GetExitStatus() << "\n";
						}

						done = true;
					}
					else if (state == lldb::eStateCrashed)
					{
						Out::Verb() << "Process crashed.\n";
						done = true;
					}
					else if (state == lldb::eStateDetached)
					{
						Out::Verb() << "Process detached.\n";
						done = true;
					}
					else if (state == lldb::eStateUnloaded)
					{
						Out::Verb() << "ERROR: Process unloaded!\n";
						done = true;
					}
					else if (state == lldb::eStateConnected)
					{
						Out::Verb() << "Process connected.\n";
					}
					else if (state == lldb::eStateAttaching)
					{
						Out::Verb() << "Process attaching.\n";
					}
					else if (state == lldb::eStateLaunching)
					{
						Out::Verb() << "Process launching.\n";
					}
				}
			}
			else
			{
				Out::Verb() << "Event: " << lldb::SBEvent::GetCStringFromEvent(event) << "\n";
			}
		}
		else
		{
			Out::Verb() << "Process event has not occured in the last" << timeOut <<
				" seconds, killing the process ...\n";
			done = true;
		}
	}

	// Clean up.
	process.Kill();
	debugger.DeleteTarget(target);
	lldb::SBDebugger::Destroy(debugger);

	return false;
}

/**
 * Prints the expected number of iterations, the actual number of iterations,
 * the original size of the input file and the size of the output file.
 *
 * @param stats The instance of the structure keeping track of the current run.
 */
void DisplayStats(Statistics& stats)
{
	Out::All() << "===------------------------ Reduction statistics ------------------------===\n";

	Out::All() << "Expected iterations:          " << stats.expectedIterations << "\n";
	Out::All() << "Actual iterations:             " << stats.totalIterations << "\n";
	Out::All() << "Original size [bytes]:        " << stats.inputSizeInBytes << "\n";
	Out::All() << "Size of the result [bytes]:   " << stats.outputSizeInBytes << "\n";

	Out::All() << "===----------------------------------------------------------------------===\n";
}

/**
 * Dumps the content of a given file to the standard output.
 *
 * @param filePath The file to be read and dumped to stdout.
 */
void PrintResult(const std::string& filePath)
{
	const std::ifstream ifs(filePath);

	if (ifs)
	{
		Out::All() << "===------------------------------- Result -------------------------------===\n";
		Out::All() << ifs.rdbuf();
		Out::All() << "===----------------------------------------------------------------------===\n";
	}
}

/**
 * Attempts to validate results of the last epoch.\n
 * Searches the temporary directory for any files, sorts them by smallest and then validates them.\n
 * The validation consists of compilation and a debugging session. Whenever the compilation succeeds,
 * the program is run using LLDB to determine whether it produces the desired runtime error.\n
 * If a valid variant is found, it is stored as `autoPieOut.<extensions based on language>` in the
 * temporary directory.
 *
 * @param context The global context of the tool required for language options and line number adjustments.
 * @return True if the epoch produced a valid result, false otherwise.
 */
bool ValidateResults(GlobalContext& context)
{
	// Collect the results.
	std::vector<std::filesystem::directory_entry> files;
	for (const auto& entry : std::filesystem::directory_iterator(TempFolder))
	{
		files.emplace_back(entry);
	}

	// Sort the output by size and iterate it from the smallest to the largest file. The first valid file is the minimal version.
	std::sort(files.begin(), files.end(),
	          [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) -> bool
	          {
		          return a.file_size() < b.file_size();
	          });

	std::optional<std::string> resultFound{};

	// Attempt to compile each file. If successful, run it in LLDB and validate the error message and location.
	for (const auto& entry : files)
	{
		if (ValidateVariant(context, entry))
		{
			resultFound = entry.path().string();
			break;
		}
	}

	if (!resultFound.has_value())
	{
		return false;
	}

	Out::All() << "Found the smallest error-inducing source file: " << resultFound.value() << "\n";

	const auto newFileName = TempFolder + std::string("autoPieOut") + LanguageToExtension(context.language);

	Out::All() << "Changing the file path to '" << newFileName << "'\n";

	std::filesystem::rename(resultFound.value(), newFileName);

	PrintResult(newFileName);

	context.stats.Finalize(newFileName);
	DisplayStats(context.stats);

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

/**
 * Converts Clang's language enum into a readable string form.\n
 * Used for pretty printing the language messages.
 *
 * @param lang The language to be converted.
 * @return The human-readable string corresponding to the given language.
 */
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

/**
 * Converts supported languages into their mainstream file extensions.\n
 * Note that only a single extension could be used => no .cc for C++ files.
 *
 * @param lang The language whose extension should be extracted.
 * @return The extension of the language's source files, starting with a dot.
 */
std::string LanguageToExtension(const clang::Language lang)
{
	switch (lang)
	{
	case clang::Language::C:
		return ".c";
	case clang::Language::CXX:
		return ".cpp";
	default:
		throw std::invalid_argument("Language not supported.");
	}
}
