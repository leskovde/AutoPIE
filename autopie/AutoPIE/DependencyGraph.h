#pragma once
#include <queue>

class DependencyGraph
{
	std::unordered_map<int, std::vector<int>> edges_;

public:
	DependencyGraph()
	{
		edges_ = std::unordered_map<int, std::vector<int>>();
	}

	void InsertDependency(const int parent, const int child)
	{
		auto it = edges_.find(parent);

		if (it == edges_.end())
		{
			it = edges_.insert(std::pair<int, std::vector<int>>(parent, std::vector<int>())).first;
		}

		it->second.push_back(child);
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
