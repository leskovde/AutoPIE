#include <llvm/ADT/ArrayRef.h>
#include <llvm/Object/MachO.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Host.h>

#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>

#include <lldb/API/SBDebugger.h>
#include <lldb/API/SBError.h>
#include <lldb/API/SBEvent.h>
#include <lldb/API/SBListener.h>
#include <lldb/API/SBProcess.h>
#include <lldb/API/SBTarget.h>
#include <lldb/API/SBThread.h>

#include <filesystem>
#include <optional>

#include "../include/Actions.h"
#include "../include/Context.h"
#include "../include/Helper.h"
#include "../include/Options.h"
#include "../include/Streams.h"

using namespace llvm;

/**
 * Guards the LLDB module, destroying it upon exiting the scope.
 */
struct LLDBSentry
{
	/**
	 * Initializes the LLDB debugger.
	 */
	LLDBSentry()
	{
		lldb::SBDebugger::Initialize();
	}

	/**
	 * Destroys the LLDB debugger.
	 */
	~LLDBSentry()
	{
		lldb::SBDebugger::Terminate();
	}

	// Rule of three.
	
	LLDBSentry(const LLDBSentry& other) = delete;
	LLDBSentry& operator=(const LLDBSentry& other) = delete;
};

bool ValidateResults(const clang::Language inputLanguage)
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
		const auto compilationExitCode = Compile(entry, inputLanguage);

		if (compilationExitCode != 0)
		{
			// File could not be compiled, continue.
			continue;
		}

		// Keep all LLDB logic written explicitly, not refactored in a function.
		// The function could be called when the LLDBSentry is not initialized => unwanted behaviour.

		// Create a debugger object - represents an instance of LLDB.
		auto debugger(lldb::SBDebugger::Create());

		if (!debugger.IsValid())
		{
			errs() << "Debugger could not be created.\n";
			exit(EXIT_FAILURE);
		}

		const char* arguments[] = {nullptr};

		lldb::SBError error;
		lldb::SBLaunchInfo launchInfo(arguments);

		launchInfo.SetWorkingDirectory(TempFolder);
		launchInfo.SetLaunchFlags(lldb::eLaunchFlagExec | lldb::eLaunchFlagDebug);

		const auto executable = TempFolder + entry.path().filename().replace_extension(".exe").string();
		out::Verb() << "\nLLDB Target creation for " << executable << " ...\n";

		// Create and launch a target - represents a debugging session of a single executable.
		// Launching creates a new process in which the executable is ran.
		auto target = debugger.CreateTarget(executable.c_str(), sys::getDefaultTargetTriple().c_str(), "", true, error);

		out::Verb() << "error.Success()              = " << static_cast<int>(error.Success()) << "\n";
		out::Verb() << "target.IsValid()             = " << static_cast<int>(target.IsValid()) << "\n";

		out::Verb() << "\nLLDB Process launch ...\n";

		auto process = target.Launch(launchInfo, error);
		out::Verb() << "error.Success()              = " << static_cast<int>(error.Success()) << "\n";
		out::Verb() << "process.IsValid()            = " << static_cast<int>(process.IsValid()) << "\n";
		out::Verb() << "process.GetProcessID()       = " << process.GetProcessID() << "\n";
		out::Verb() << "process.GetState()           = " << StateToString(process.GetState()) << "\n";
		out::Verb() << "process.GetNumThreads()      = " << process.GetNumThreads() << "\n";

		auto listener = debugger.GetListener();
		out::Verb() << "listener.IsValid()           = " << static_cast<int>(listener.IsValid()) << "\n";

		auto done = false;
		const auto timeOut = 360;
		
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
						out::Verb() << "Invalid process event: " << StateToString(state) << "\n";
					}
					else
					{
						out::Verb() << "Process state event changed to: " << StateToString(state) << "\n";

						if (state == lldb::eStateStopped)
						{
							// The debugger stopped at a breakpoint. Since no breakpoints were set, this is most likely an exception.
							// Analyze the current stack frame to determine the status and position of the error.
							
							out::Verb() << "Stopped at a breakpoint.\n";
							out::Verb() << "LLDB Threading ...\n";

							auto thread = process.GetSelectedThread();
							out::Verb() << "thread.IsValid()             = " << static_cast<int>(thread.IsValid()) << "\n";
							out::Verb() << "thread.GetThreadID()         = " << thread.GetThreadID() << "\n";
							out::Verb() << "thread.GetName()             = " << (thread.GetName() != nullptr
								                                                     ? thread.GetName()
								                                                     : "(null)") << "\n";
							out::Verb() << "thread.GetStopReason()       = " << StopReasonToString(thread.GetStopReason()) <<
								"\n";
							out::Verb() << "thread.IsSuspended()         = " << static_cast<int>(thread.IsSuspended()) <<
								"\n";
							out::Verb() << "thread.IsStopped()           = " << static_cast<int>(thread.IsStopped()) << "\n";
							out::Verb() << "process.GetState()           = " << StateToString(process.GetState()) << "\n";

							if (thread.GetStopReason() == lldb::StopReason::eStopReasonException)
							{
								out::Verb() << "An exception was hit, killing the process ...\n";
								done = true;
							}

							auto frame = thread.GetSelectedFrame();
							out::Verb() << "frame.IsValid()              = " << static_cast<int>(frame.IsValid()) << "\n";

							auto function = frame.GetFunction();

							if (function.IsValid())
							{
								out::Verb() << "function.GetDisplayName()   = " << (function.GetDisplayName() != nullptr
									                                                    ? function.GetDisplayName()
									                                                    : "(null)") << "\n";
							}

							auto symbol = frame.GetSymbol();
							out::Verb() << "symbol.IsValid()             = " << static_cast<int>(symbol.IsValid()) << "\n";

							if (symbol.IsValid())
							{
								out::Verb() << "symbol.GetDisplayName()      = " << symbol.GetDisplayName() << "\n";

								auto symbolContext = frame.GetSymbolContext(lldb::eSymbolContextLineEntry);

								out::Verb() << "symbolContext.IsValid()      = " << static_cast<int>(symbolContext.IsValid())
									<< "\n";

								if (symbolContext.IsValid())
								{
									const auto fileName = symbolContext.GetLineEntry().GetFileSpec().GetFilename();
									const auto lineNumber = symbolContext.GetLineEntry().GetLine();
									
									out::Verb() << "symbolContext.GetFilename()  = " << fileName << "\n";
									out::Verb() << "symbolContext.GetLine()      = " << lineNumber << "\n";
									out::Verb() << "symbolContext.GetColumn()    = " << symbolContext
									                                                    .GetLineEntry().GetColumn() << "\n";

									// TODO(Denis): Confirm the location.
									
									// TODO: Confirm the message.

									if (lineNumber == 4 || lineNumber == 2)
									{
										resultFound.emplace(entry.path().string());
										done = true;
									}
									
									// TODO: The location must be adjusted to the reduction - probably something that should be done inside a visitor.
								}
							}

							process.Continue();
						}
						else if (state == lldb::eStateExited)
						{
							out::Verb() << "Process exited.\n";

							auto description = process.GetExitDescription();

							if (description != nullptr)
							{
								out::Verb() << "Exit status " << process.GetExitStatus() << ":" << description << "\n";
							}
							else
							{
								out::Verb() << "Exit status " << process.GetExitStatus() << "\n";
							}

							done = true;
						}
						else if (state == lldb::eStateCrashed)
						{
							out::Verb() << "Process crashed.\n";
							done = true;
						}
						else if (state == lldb::eStateDetached)
						{
							out::Verb() << "Process detached.\n";
							done = true;
						}
						else if (state == lldb::eStateUnloaded)
						{
							out::Verb() << "ERROR: Process unloaded!\n";
							done = true;
						}
						else if (state == lldb::eStateConnected)
						{
							out::Verb() << "Process connected.\n";
						}
						else if (state == lldb::eStateAttaching)
						{
							out::Verb() << "Process attaching.\n";
						}
						else if (state == lldb::eStateLaunching)
						{
							out::Verb() << "Process launching.\n";
						}
					}
				}
				else
				{
					out::Verb() << "Event: " << lldb::SBEvent::GetCStringFromEvent(event) << "\n";
				}
			}
			else
			{
				out::Verb() << "Process event has not occured in the last" << timeOut << " seconds, killing the process ...\n";
				done = true;
			}
		}

		process.Kill();
		debugger.DeleteTarget(target);
		lldb::SBDebugger::Destroy(debugger);

		if (resultFound.has_value())
		{
			break;
		}
	}

	// TODO: Conclude the result, print statistics (reduction rate, time consumed, variants created, variants compiled, etc.).

	if (!resultFound.has_value())
	{
		return false;
	}
	
	out::All() << "Found the smallest error-inducing source file: " << resultFound.value() << "\n";
	out::All() << "Renaming the file to '"<< TempFolder << "autoPieOut.c'\n";
	
	std::filesystem::rename(resultFound.value(), TempFolder + std::string("autoPieOut.c"));
	
	return true;
}

/**
 * Generates a minimal program variant by naively removing statements.
 * 
 * Call:\n
 * > NaiveReduction.exe [file with error] [line with error] [error description message] [reduction ratio] <source path 0> [... <source path N>] --
 * e.g. NaiveReduction.exe --loc-file="example.cpp" --loc-line=17 --error-message="segmentation fault" --ratio=0.5 example.cpp --
 */
int main(int argc, const char** argv)
{
	// Parse the command-line args passed to the tool.
	clang::tooling::CommonOptionsParser op(argc, argv, Args);

	// TODO(Denis): Create multi-file support.
	if (op.getSourcePathList().size() > 1)
	{
		errs() << "Only a single source file is supported.\n";
		return EXIT_FAILURE;
	}
	
	auto parsedInput = InputData(static_cast<std::basic_string<char>>(ErrorMessage),
	                             Location(static_cast<std::basic_string<char>>(SourceFile), LineNumber), ReductionRatio,
	                             DumpDot);

	// Prompt the user to clear the temp directory.
	if (!ClearTempDirectory(false))
	{
		errs() << "Terminating...\n";
		return EXIT_FAILURE;
	}

	const auto epochCount = 5;
	
	auto context = GlobalContext(parsedInput, *op.getSourcePathList().begin(), epochCount);
	clang::tooling::ClangTool tool(op.getCompilations(), context.parsedInput.errorLocation.filePath);

	auto inputLanguage = clang::Language::Unknown;
	// Run a language check inside a separate scope so that all built ASTs get freed at the end.
	{
		std::vector<std::unique_ptr<clang::ASTUnit>> trees;
		tool.buildASTs(trees);

		if (trees.size() != 1)
		{
			errs() << "Although only one AST was expected, " << trees.size() << " ASTs have been built.\n";
			return EXIT_FAILURE;
		}
		
		inputLanguage = (*trees.begin())->getInputKind().getLanguage();
		
		out::Verb() << "File: " << (*trees.begin())->getOriginalSourceFileName() << ", language: " << LanguageToString(inputLanguage) << "\n";
	}

	assert(inputLanguage != clang::Language::Unknown);

	if (!CheckLocationValidity(parsedInput.errorLocation.filePath, parsedInput.errorLocation.lineNumber))
	{
		errs() << "The specified error location is invalid!\nSource path: " << parsedInput.errorLocation.filePath
			<< ", line: " << parsedInput.errorLocation.lineNumber << " could not be found.\n";
	}

	LLDBSentry sentry;
	
	for (auto i = 0; i < context.deepeningContext.epochCount; i++)
	{
		context.currentEpoch = i;
		
		// Run all Clang AST related actions.
		auto result = tool.run(CustomFrontendActionFactory(context).get());

		if (result)
		{
			errs() << "The tool returned a non-standard value: " << result << "\n";
		}
		
		if (ValidateResults(inputLanguage))
		{
			return EXIT_SUCCESS;
		}
		
		out::All() << "Epoch " << i + 1 << " out of " << epochCount << ": A smaller program variant could not be found.\n";
		ClearTempDirectory(false);
	}

	return EXIT_FAILURE;
}
