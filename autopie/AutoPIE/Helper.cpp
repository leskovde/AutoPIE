#include <clang/Basic/SourceLocation.h>
#include <clang/AST/Stmt.h>
#include <stdexcept>

#include "Helper.h"

clang::SourceRange GetSourceRange(const clang::Stmt& s)
{
	return { s.getBeginLoc(), s.getEndLoc() };
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
	std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);

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

		if (decision == 'n' || decision == 'N')
		{
			return false;
		}
	}

	llvm::outs() << "Clearing the 'temp/' directory...";

	std::filesystem::remove_all("temp/");
	std::filesystem::create_directory("temp");

	return true;
}