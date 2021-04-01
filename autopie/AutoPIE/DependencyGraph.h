#pragma once
#include <queue>

class DependencyGraph
{
	std::unordered_map<int, std::vector<int>> edges_;
	std::unordered_map<int, std::string> debugCodeSnippets_;

public:
	DependencyGraph()
	{
		edges_ = std::unordered_map<int, std::vector<int>>();
	}

	void InsertDependency(const int parent, const int child)
	{
		if (parent == child)
		{
			return;
		}
			
		auto it = edges_.find(parent);

		if (it == edges_.end())
		{
			it = edges_.insert(std::pair<int, std::vector<int>>(parent, std::vector<int>())).first;
		}

		it->second.push_back(child);
	}

	void InsertCodeSnippetForDebugging(const int node, const std::string& snippet)
	{
		const auto success = debugCodeSnippets_.insert(std::pair<int, std::string>(node, snippet)).second;

		if (!success)
		{
			llvm::outs() << "DEBUG: Could not add a snippet to the mapping. The snippet:\n" << snippet << "\n";
		}
	}

	void PrintGraphForDebugging() const
	{
		llvm::outs() << "--------------- Dependency graph and its code ---------------\n";

		for (auto it = debugCodeSnippets_.cbegin(); it != debugCodeSnippets_.cend(); ++it)
		{
			llvm::outs() << "Node " << it->first << ":\n" << it->second << "\n";
		}
		
		llvm::outs() << "-------------------------------------------------------------\n";
	}

	std::vector<int> GetDependentNodes(const int startingNode)
	{
		auto nodeQ = std::queue<int>();
		auto allDependencies = std::vector<int>();

		nodeQ.push(startingNode);

		while (!nodeQ.empty())
		{
			auto currentNode = nodeQ.front();
			nodeQ.pop();
			
			auto it = edges_.find(currentNode);

			if (it != edges_.end())
			{
				for (auto dependency : it->second)
				{
					nodeQ.push(dependency);
					allDependencies.push_back(dependency);
				}
			}
		}

		return allDependencies;
	}
};
