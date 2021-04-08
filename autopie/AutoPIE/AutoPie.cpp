#include <iostream>
#include <filesystem>

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/None.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Triple.h"
#include "llvm/ADT/Twine.h"
#include "llvm/BinaryFormat/MachO.h"
#include "llvm/Object/Error.h"
#include "llvm/Object/MachO.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/SymbolicFile.h"
#include "llvm/Support/DataExtractor.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/LEB128.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SwapByteOrder.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/FileSystemOptions.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Basic/TargetInfo.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/CompilerInvocation.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Lex/HeaderSearch.h>
#include <clang/Lex/HeaderSearchOptions.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Lex/PreprocessorOptions.h>
#include <clang/Parse/ParseAST.h>
#include <clang/Sema/Sema.h>

#include "llvm/ADT/Triple.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/CodeGen/CodeGenAction.h"
#include "lldb/API/SBDebugger.h"
#include "lldb/API/SBError.h"
#include "lldb/API/SBTarget.h"

#include "Context.h"
#include "Helper.h"
#include "Actions.h"

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
	cl::desc("(Specifies whether a GraphViz file containing relationships of code units should be created.)"),
	cl::init(false),
	cl::cat(Args));

bool Validate(const char* const userInputError, const std::filesystem::directory_entry& entry)
{
	outs() << "ENTRY: " << entry.path().string() << "\n";
	const std::string compileCommand = "g++ -o temp\\" + entry.path().filename().replace_extension(".exe").string() + " " +
		entry.path().string() + " 2>&1";

	const auto compilationResult = ExecCommand(compileCommand.c_str());
	outs() << "COMPILATION: " << compilationResult << "\n";

	if (compilationResult.find("error") == std::string::npos)
	{
		const std::string debugCommand = "gdb --batch -ex run temp\\" + entry
		                                                          .path().filename().replace_extension(".exe").string()
			+ " 2>&1";

		const auto debugResult = ExecCommand(debugCommand.c_str());
		outs() << "DEBUG: " << debugResult << "\n";

		if (debugResult.find(userInputError) != std::string::npos)
		{
			copy_file(entry, "result.cpp");
			return true;
		}
	}
	return false;
}



Module* CompileInMemory(const char* fileName)
{
	const std::vector<const char*> rawArguments = { fileName };
	const ArrayRef<const char*> arguments = rawArguments;

	clang::DiagnosticOptions diagnosticOptions;
	auto textDiagnosticPrinter = std::make_unique<clang::TextDiagnosticPrinter>(outs(), &diagnosticOptions);
	IntrusiveRefCntPtr<clang::DiagnosticIDs> diagIDs;

	auto diagnosticsEngine = std::make_unique<clang::DiagnosticsEngine>(diagIDs, &diagnosticOptions, textDiagnosticPrinter.get());

	clang::CompilerInstance compilerInstance;
	auto& compilerInvocation = compilerInstance.getInvocation();

	clang::CompilerInvocation::CreateFromArgs(compilerInvocation, rawArguments, *diagnosticsEngine.release());
	
	auto* languageOptions = compilerInvocation.getLangOpts();
	auto& preprocessorOptions = compilerInvocation.getPreprocessorOpts();
	auto& targetOptions = compilerInvocation.getTargetOpts();
	auto& frontEndOptions = compilerInvocation.getFrontendOpts();
	frontEndOptions.ShowStats = true;
	auto& headerSearchOptions = compilerInvocation.getHeaderSearchOpts();
	headerSearchOptions.Verbose = true;
	auto& codeGenOptions = compilerInvocation.getCodeGenOpts();

	//frontEndOptions.Inputs.clear();
	//TODO: Different languages require different handling!
	//frontEndOptions.Inputs.push_back(clang::FrontendInputFile(fileName, clang::InputKind(clang::Language::C)));

	targetOptions.Triple = sys::getDefaultTargetTriple();
	compilerInstance.createDiagnostics(textDiagnosticPrinter.get(), false);

	LLVMContext context;

	const std::unique_ptr<clang::CodeGenAction> action = std::make_unique<clang::EmitLLVMOnlyAction>(&context);
	
	if (!compilerInstance.ExecuteAction(*action))
	{
		// Failed to compile, and should display on cout the result of the compilation
	}

	return nullptr;
	
	/*
	const ArrayRef<const char*> arguments = { "-o", "temp/seg", fileName };
	
	const auto diagIDs = IntrusiveRefCntPtr<clang::DiagnosticIDs>(std::make_unique<clang::DiagnosticIDs>().get());
	const auto diagOptions = IntrusiveRefCntPtr<clang::DiagnosticOptions>(std::make_unique<clang::DiagnosticOptions>().get());
	const auto printer = std::make_unique<clang::TextDiagnosticPrinter>(errs(), diagOptions.get());
	clang::DiagnosticsEngine diags(diagIDs, diagOptions, printer.get());

	const auto invocation = std::make_shared<clang::CompilerInvocation>();
	clang::CompilerInvocation::CreateFromArgs(*invocation, arguments, diags);

	clang::CompilerInstance compiler;
	compiler.setInvocation(invocation);
	compiler.setDiagnostics(&diags);

	if (!compiler.hasDiagnostics())
	{
		return nullptr;
	}

	auto action = std::unique_ptr<clang::CodeGenAction>(std::make_unique<clang::EmitLLVMOnlyAction>());

	if (!compiler.ExecuteAction(*action))
	{
		return nullptr;
	}

	auto result = action->takeModule();
	errs() << result.get();

	return result.release();
	*/
}

class LLDBSentry {
public:
	LLDBSentry() {
		// Initialize LLDB
		lldb::SBDebugger::Initialize();
	}
	~LLDBSentry() {
		// Terminate LLDB
		lldb::SBDebugger::Terminate();
	}
};

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

	// TODO: Create multi-file support.
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
		[](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry b) -> bool
		{
			return a.file_size() < b.file_size();
		});

	const auto biggest = files.back();

	auto compilation = Compile(biggest.path().string().c_str());
	
	outs() << std::boolalpha;
	
	LLDBSentry sentry;
	auto debugger(lldb::SBDebugger::Create());

	if (!debugger.IsValid())
	{
		errs() << "Debugger could not be created.\n";
		return EXIT_FAILURE;
	}

	outs() << "LLDB Target creation ...\n";

	lldb::SBError error;
	//auto target = debugger.CreateTarget()

	outs() << "LLDB Process launch ...\n";
	
	return EXIT_SUCCESS;
}
