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

#include "../include/Actions.h"
#include "../include/Context.h"
#include "../include/Helper.h"
#include <optional>

using namespace llvm;

static cl::OptionCategory Args("AutoPIE Options");

/**
 * Defines a --help option for the tool. Shows all available options and their descriptions.\n
 * Although the variable is unused, it is still parsed in the CommonOptionsParser.
 */
static cl::extrahelp CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);

/**
 * Defines additional text to the CommonHelp. The text is shown when using --help.\n
 * Although the variable is unused, it is still parsed in the CommonOptionsParser.
 */
static cl::extrahelp MoreHelp("\nMore help text...");

/**
 * Specifies the source file in which an error was found.\n
 * The value is later used for location confirmation.
 */
static cl::opt<std::string> SourceFile("loc-file",
                                       cl::desc("The name of the file in which the error occured."),
                                       cl::init(""),
                                       cl::cat(Args));

/**
 * Specifies the line number in the previously specified source file on which an error was found.\n
 * The value is later used for location confirmation.
 */
static cl::opt<int> LineNumber("loc-line",
                               cl::desc("The line number on which the error occured."),
                               cl::init(0),
                               cl::cat(Args));

/**
 * A description of a runtime error. The description should be reproducible, e.g., 'segmentation fault'.\n
 * It is assumed that the description is a substring of the stack trace / exception message seen after launch.\n
 * The value is later used for location confirmation.
 */
static cl::opt<std::string> ErrorMessage("error-message",
                                         cl::desc("A part of the error message specifying the nature of the error."),
                                         cl::init(""),
                                         cl::cat(Args));

// TODO: Implement reduction ratio to cut the variant search.
/**
 * Specifies the desired best-possible size of the output. Values are between 0 and 1, with 0 being the unreduced
 * (original) file and 1 being an attempt to fully reduce the source file.\n
 * The value is used to stop the variant search upon hitting a certain threshold.\n
 * E.g., 0.75 => the output file will have (in the best scenario) roughly a quarter its size in bytes,
 * with three quarters being removed.
 */
static cl::opt<double> ReductionRatio("ratio",
                                      cl::desc("Limits the reduction to a specific ratio between 0 and 1."),
                                      cl::init(1.0),
                                      cl::cat(Args));

/**
 * If set to true, the program generates a .dot file containing a graph of code units.\n
 * The file serves to visualize the term 'code units' and also shows dependencies in the source code.\n
 * The value is used after the first few AST passes to check whether an output .dot file should be generated.
 */
static cl::opt<bool> DumpDot("dump-dot",
                             cl::desc(
	                             "Specifies whether a GraphViz file containing relationships of code units should be created."),
                             cl::init(false),
                             cl::cat(Args));

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

	auto context = GlobalContext(parsedInput, *op.getSourcePathList().begin());
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
		
		outs() << "File: " << (*trees.begin())->getOriginalSourceFileName() << ", language: " << LanguageToString(inputLanguage) << "\n";
	}

	assert(inputLanguage != clang::Language::Unknown);

	if (!CheckLocationValidity(parsedInput.errorLocation.filePath, parsedInput.errorLocation.lineNumber))
	{
		errs() << "The specified error location is invalid!\nSource path: " << parsedInput.errorLocation.filePath
			<< ", line: " << parsedInput.errorLocation.lineNumber << " could not be found.\n";
	}
	
	// Run all Clang AST related actions.
	auto result = tool.run(CustomFrontendActionFactory(context).get());
	
	if (result)
	{
		outs() << "The tool returned a non-standard value: " << result << "\n";
	}
	
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

	LLDBSentry sentry;
	std::optional<std::string> resultFound{};
	
	// Attempt to compile each file. If successful, run it in LLDB and validate the error message and location.
	for (const auto& entry : files)
	{
		auto compilationExitCode = Compile(entry, inputLanguage);

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
			return EXIT_FAILURE;
		}

		const char* arguments[] = {nullptr};

		lldb::SBError error;
		lldb::SBLaunchInfo launchInfo(arguments);

		launchInfo.SetWorkingDirectory(TempFolder);
		launchInfo.SetLaunchFlags(lldb::eLaunchFlagExec | lldb::eLaunchFlagDebug);

		const auto executable = TempFolder + entry.path().filename().replace_extension(".exe").string();
		outs() << "\nLLDB Target creation for " << executable << " ...\n";

		// Create and launch a target - represents a debugging session of a single executable.
		// Launching creates a new process in which the executable is ran.
		auto target = debugger.CreateTarget(executable.c_str(), sys::getDefaultTargetTriple().c_str(), "", true, error);

		outs() << "error.Success()              = " << static_cast<int>(error.Success()) << "\n";
		outs() << "target.IsValid()             = " << static_cast<int>(target.IsValid()) << "\n";

		outs() << "\nLLDB Process launch ...\n";

		auto process = target.Launch(launchInfo, error);
		outs() << "error.Success()              = " << static_cast<int>(error.Success()) << "\n";
		outs() << "process.IsValid()            = " << static_cast<int>(process.IsValid()) << "\n";
		outs() << "process.GetProcessID()       = " << process.GetProcessID() << "\n";
		outs() << "process.GetState()           = " << StateToString(process.GetState()) << "\n";
		outs() << "process.GetNumThreads()      = " << process.GetNumThreads() << "\n";

		auto listener = debugger.GetListener();
		outs() << "listener.IsValid()           = " << static_cast<int>(listener.IsValid()) << "\n";

		auto done = false;

		// The debugger is set to run asynchronously (debugger.GetAsync() => true).
		// The communication is done via events. Listen for events broadcast by the forked process.
		// Events are handled depending on the state of the process, the most important is `eStateStopped`
		// (stopped at a breakpoint, such as an exception) and `eStateExited` (ran without errors).
		while (!done)
		{
			lldb::SBEvent event;

			if (listener.WaitForEvent(/*Waiting timeout:*/ 360 /*seconds.*/, event))
			{
				if (lldb::SBProcess::EventIsProcessEvent(event))
				{
					auto state = lldb::SBProcess::GetStateFromEvent(event);

					if (state == lldb::eStateInvalid)
					{
						outs() << "Invalid process event: " << StateToString(state) << "\n";
					}
					else
					{
						outs() << "Process state event changed to: " << StateToString(state) << "\n";

						if (state == lldb::eStateStopped)
						{
							// The debugger stopped at a breakpoint. Since no breakpoints were set, this is most likely an exception.
							// Analyze the current stack frame to determine the status and position of the error.
							
							outs() << "Stopped at a breakpoint.\n";
							outs() << "LLDB Threading ...\n";

							auto thread = process.GetSelectedThread();
							outs() << "thread.IsValid()             = " << static_cast<int>(thread.IsValid()) << "\n";
							outs() << "thread.GetThreadID()         = " << thread.GetThreadID() << "\n";
							outs() << "thread.GetName()             = " << (thread.GetName() != nullptr
								                                                ? thread.GetName()
								                                                : "(null)") << "\n";
							outs() << "thread.GetStopReason()       = " << StopReasonToString(thread.GetStopReason()) <<
								"\n";
							outs() << "thread.IsSuspended()         = " << static_cast<int>(thread.IsSuspended()) <<
								"\n";
							outs() << "thread.IsStopped()           = " << static_cast<int>(thread.IsStopped()) << "\n";
							outs() << "process.GetState()           = " << StateToString(process.GetState()) << "\n";

							if (thread.GetStopReason() == lldb::StopReason::eStopReasonException)
							{
								outs() << "An exception was hit, killing the process ...\n";
								done = true;
							}

							auto frame = thread.GetSelectedFrame();
							outs() << "frame.IsValid()              = " << static_cast<int>(frame.IsValid()) << "\n";

							auto function = frame.GetFunction();

							if (function.IsValid())
							{
								outs() << "function.GetDisplayName()   = " << (function.GetDisplayName() != nullptr
									                                               ? function.GetDisplayName()
									                                               : "(null)") << "\n";
							}

							auto symbol = frame.GetSymbol();
							outs() << "symbol.IsValid()             = " << static_cast<int>(symbol.IsValid()) << "\n";

							if (symbol.IsValid())
							{
								outs() << "symbol.GetDisplayName()      = " << symbol.GetDisplayName() << "\n";

								auto symbolContext = frame.GetSymbolContext(lldb::eSymbolContextLineEntry);

								outs() << "symbolContext.IsValid()      = " << static_cast<int>(symbolContext.IsValid())
									<< "\n";

								if (symbolContext.IsValid())
								{
									const auto fileName = symbolContext.GetLineEntry().GetFileSpec().GetFilename();
									const auto lineNumber = symbolContext.GetLineEntry().GetLine();
									
									outs() << "symbolContext.GetFilename()  = " << fileName << "\n";
									outs() << "symbolContext.GetLine()      = " << lineNumber << "\n";
									outs() << "symbolContext.GetColumn()    = " << symbolContext
									                                               .GetLineEntry().GetColumn() << "\n";

									// TODO(Denis): Confirm the location.
									
									// TODO: Confirm the message.

									if (lineNumber == 4)
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
							outs() << "Process exited.\n";

							auto description = process.GetExitDescription();

							if (description != nullptr)
							{
								outs() << "Exit status " << process.GetExitStatus() << ":" << description << "\n";
							}
							else
							{
								outs() << "Exit status " << process.GetExitStatus() << "\n";
							}

							done = true;
						}
						else if (state == lldb::eStateCrashed)
						{
							outs() << "Process crashed.\n";
							done = true;
						}
						else if (state == lldb::eStateDetached)
						{
							outs() << "Process detached.\n";
							done = true;
						}
						else if (state == lldb::eStateUnloaded)
						{
							errs() << "ERROR: Process unloaded!\n";
							done = true;
						}
						else if (state == lldb::eStateConnected)
						{
							outs() << "Process connected.\n";
						}
						else if (state == lldb::eStateAttaching)
						{
							outs() << "Process attaching.\n";
						}
						else if (state == lldb::eStateLaunching)
						{
							outs() << "Process launching.\n";
						}
					}
				}
				else
				{
					outs() << "Event: " << lldb::SBEvent::GetCStringFromEvent(event) << "\n";
				}
			}
			else
			{
				outs() << "Process event has not occured in the last 10 seconds, killing the process ...\n";
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
		outs() << "A smaller program variant could not be found. AutoPie will terminate without a generated result.\n";
		return EXIT_FAILURE;
	}
	
	outs() << "Found the smallest error-inducing source file: " << resultFound.value() << "\n";
	
	return EXIT_SUCCESS;
}
