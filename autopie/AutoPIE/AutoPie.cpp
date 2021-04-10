#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/Object/MachO.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/Program.h>

#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Driver/Compilation.h>
#include <clang/Driver/Driver.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
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

#include "Actions.h"
#include "Context.h"
#include "Helper.h"

using namespace llvm;

static cl::OptionCategory Args("AutoPIE Options");
static cl::extrahelp CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);
static cl::extrahelp MoreHelp("\nMore help text...");

static cl::opt<std::string> SourceFile("loc-file",
                                       cl::desc("(The name of the file in which the error occured.)"),
                                       cl::init(""),
                                       cl::cat(Args));

static cl::opt<int> LineNumber("loc-line",
                               cl::desc("(The line number on which the error occured.)"),
                               cl::init(0),
                               cl::cat(Args));

static cl::opt<std::string> ErrorMessage("error-message",
                                         cl::desc("(A part of the error message specifying the nature of the error.)"),
                                         cl::init(""),
                                         cl::cat(Args));

static cl::opt<double> ReductionRatio("ratio",
                                      cl::desc("(Limits the reduction to a specific ratio between 0 and 1.)"),
                                      cl::init(1.0),
                                      cl::cat(Args));

static cl::opt<bool> DumpDot("dump-dot",
                             cl::desc(
	                             "(Specifies whether a GraphViz file containing relationships of code units should be created.)"),
                             cl::init(false),
                             cl::cat(Args));

int Compile(const std::filesystem::directory_entry& entry)
{
	//const auto output = "temp/a.exe";
	const auto input = entry.path().string();
	const auto output = "temp\\" + entry.path().filename().replace_extension(".exe").string();
	auto clangPath = sys::findProgramByName("clang");

	std::filesystem::file_time_type lastWrite;
	/*
	if (std::filesystem::exists(output))
	{
		try
		{
			//std::filesystem::remove(output);
			lastWrite = std::filesystem::last_write_time(output);
		}
		catch (const std::filesystem::filesystem_error& error)
		{
			errs() << "Error while deleting an existing executable: " << error.what() << "\n";
		}
	}
	*/
	const auto arguments = std::vector<const char*>{
		clangPath->c_str(), /*"-v",*/ "-O0", "-g", "-o", output.c_str(), input.c_str()
	};

	clang::DiagnosticOptions diagnosticOptions;
	const auto textDiagnosticPrinter = std::make_unique<clang::TextDiagnosticPrinter>(outs(), &diagnosticOptions);
	IntrusiveRefCntPtr<clang::DiagnosticIDs> diagIDs;

	auto diagnosticsEngine = std::make_unique<clang::DiagnosticsEngine>(diagIDs, &diagnosticOptions,
	                                                                    textDiagnosticPrinter.get());

	clang::driver::Driver driver(arguments[0], sys::getDefaultTargetTriple(), *diagnosticsEngine.release());

	const auto compilation = std::unique_ptr<clang::driver::Compilation>(driver.BuildCompilation(arguments));

	//driver.PrintActions(*compilation);

	auto result = 1; // Set the initial value to invalid, since we don't need those imposter vibes in our lives.
	SmallVector<std::pair<int, const clang::driver::Command*>, 16> failingCommands;

	if (compilation)
	{
		result = driver.ExecuteCompilation(*compilation, failingCommands);
	}

	outs() << "\n";

	if (!std::filesystem::exists(output))
	{
		result |= 1;
	}

	/*
	if (std::filesystem::exists(output))
	{
		if (lastWrite == std::filesystem::last_write_time(output))
		{
			result |= 1;	
		}
	}
	else
	{
		result |= 1;
	}
	*/

	return result;
}

class LLDBSentry
{
public:
	LLDBSentry()
	{
		// Initialize LLDB
		lldb::SBDebugger::Initialize();
	}

	~LLDBSentry()
	{
		// Terminate LLDB
		lldb::SBDebugger::Terminate();
	}
};

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

/**
 * Generates a minimal program variant by naively removing statements.
 * 
 * Call:
 * > AutoPIE.exe [file with error] [line with error] [error description message] [reduction ratio] <source path 0> [... <source path N>] --
 * e.g. AutoPIE.exe --loc-file="example.cpp" --loc-line=17 --error-message="stack overflow" --ratio=0.5 example.cpp --
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

	auto parsedInput = InputData(ErrorMessage, Location(SourceFile, LineNumber), ReductionRatio, DumpDot);

	// Prompt the user to clear the temp directory.
	if (!ClearTempDirectory(false))
	{
		errs() << "Terminating...\n";
		return EXIT_FAILURE;
	}

	auto context = GlobalContext(parsedInput, *op.getSourcePathList().begin());

	clang::tooling::ClangTool tool(op.getCompilations(), context.parsedInput.errorLocation.fileName);
	auto result = tool.run(CustomFrontendActionFactory(context).get());

	//llvm_shutdown();

	std::vector<std::filesystem::directory_entry> files;

	for (const auto& entry : std::filesystem::directory_iterator("temp/"))
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

	for (const auto& entry : files)
	{
		auto compilationExitCode = Compile(entry);

		if (compilationExitCode != 0)
		{
			// File could not be compiled, continue.
			continue;
		}

		auto debugger(lldb::SBDebugger::Create());

		if (!debugger.IsValid())
		{
			errs() << "Debugger could not be created.\n";
			return EXIT_FAILURE;
		}

		//debugger.SetAsync(false);

		const char* arguments[] = {nullptr};

		lldb::SBError error;
		lldb::SBLaunchInfo launchInfo(arguments);

		launchInfo.SetWorkingDirectory("./temp");
		launchInfo.SetLaunchFlags(lldb::eLaunchFlagExec | lldb::eLaunchFlagDebug);

		const auto executable = "temp\\" + entry.path().filename().replace_extension(".exe").string();
		outs() << "\nLLDB Target creation for " << executable << " ...\n";

		auto target = debugger.CreateTarget(executable.c_str(), sys::getDefaultTargetTriple().c_str(), "", true, error);

		outs() << "error.Success()              = " << static_cast<int>(error.Success()) << "\n";
		outs() << "target.IsValid()             = " << static_cast<int>(target.IsValid()) << "\n";

		outs() << "\nLLDB Process launch ...\n";

		/*
		auto process = target.Launch(listener,
			nullptr,
			nullptr,
			nullptr,
			"temp/lldbOut.txt",
			"temp/lldbErr.txt",
			"./temp",
			lldb::eLaunchFlagExec | lldb::eLaunchFlagDebug,
			false,
			error);
		*/

		auto process = target.Launch(launchInfo, error);
		outs() << "error.Success()              = " << static_cast<int>(error.Success()) << "\n";
		outs() << "process.IsValid()            = " << static_cast<int>(process.IsValid()) << "\n";
		outs() << "process.GetProcessID()       = " << process.GetProcessID() << "\n";
		outs() << "process.GetState()           = " << StateToString(process.GetState()) << "\n";
		outs() << "process.GetNumThreads()      = " << process.GetNumThreads() << "\n";

		auto listener = debugger.GetListener();
		outs() << "listener.IsValid()           = " << static_cast<int>(listener.IsValid()) << "\n";

		auto done = false;

		while (!done)
		{
			lldb::SBEvent event;

			/*
			outs() << "listener.GetNextEvent()      = " << listener.GetNextEvent(event) << "\n";

			while (process.GetState() == lldb::eStateAttaching || process.GetState() == lldb::eStateLaunching)
			{
				outs() << "listener.GetNextEvent()      = " << listener.GetNextEvent(event) << "\n";
			}
			*/

			if (listener.WaitForEvent(360, event))
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
									outs() << "symbolContext.GetFilename()  = " << symbolContext
									                                               .GetLineEntry().GetFileSpec().
									                                               GetFilename() << "\n";
									outs() << "symbolContext.GetLine()      = " << symbolContext
									                                               .GetLineEntry().GetLine() << "\n";
									outs() << "symbolContext.GetColumn()    = " << symbolContext
									                                               .GetLineEntry().GetColumn() << "\n";

									// TODO(Denis): Confirm the location.
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

		/*
		if (process.GetState() != lldb::eStateExited && !process.Kill().Success())
		{
			errs() << "LLDB could not kill the debugging process.\n";
			return EXIT_FAILURE;
		}
		*/

		/*
		process.Kill();
		process.Clear();
		process.Destroy();
		process.Detach();
		debugger.DeleteTarget(target);
		*/
	}

	/*
	auto exception = thread.GetCurrentException();
	outs() << "exception.IsValid()              = " << exception.IsValid() << "\n";
	outs() << "exception.GetTypeName()          = " << (exception.GetTypeName() ? exception.GetTypeName() : "(null)" ) << "\n";
	outs() << "exception.GetDisplayTypeName()   = " << (exception.GetDisplayTypeName() ? exception.GetDisplayTypeName() : "(null)" ) << "\n";
	outs() << "exception.GetLocation()          = " << (exception.GetLocation() ? exception.GetLocation() : "(null)") << "\n";
	*/

	return EXIT_SUCCESS;
}
