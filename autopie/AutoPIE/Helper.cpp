#include <clang/Basic/SourceLocation.h>
#include <clang/AST/Stmt.h>
#include <stdexcept>

#include "Helper.h"
#include "DependencyGraph.h"

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

bool IsFull(std::vector<bool>& bitfield)
{
	for (const auto& bit : bitfield)
	{
		if (!bit)
		{
			return false;
		}
	}

	return true;
}

static std::string Stringify(std::vector<bool>& bitfield)
{
	std::string bits;

	for (auto bit : bitfield)
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

void Increment(std::vector<bool>& bitfield)
{
	// TODO: Write unit tests for this function (and the all variant generation).

	llvm::outs() << "DEBUG: Before inc: " << Stringify(bitfield) << "\n";
	
	for (auto it = bitfield.rbegin(); it != bitfield.rend(); ++it)
	{
		if (*it)
		{
			it->flip();
		}
		else
		{
			it->flip();
			break;
		}
	}

	llvm::outs() << "DEBUG: After inc: " << Stringify(bitfield) << "\n";
}

bool IsValid(std::vector<bool>& bitfield, DependencyGraph& dependencies)
{
	for (size_t i = 0; i < bitfield.size(); i++)
	{
		auto children = dependencies.GetDependentNodes(i);

		for (auto child : children)
		{
			if (bitfield[i] != bitfield[child])
			{
				return false;
			}
		}
	}

	return true;
}