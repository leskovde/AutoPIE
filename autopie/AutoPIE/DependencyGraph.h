#ifndef DEPENDENCYGRAPH_H
#define DEPENDENCYGRAPH_H
#pragma once
#include "Helper.h"

#include <fstream>
#include <queue>

#include <utility>

struct Node
{
	int astId{0};
	int number{0};
	std::string dumpColor;
	std::string codeSnippet;
	std::string nodeTypeName;

	Node() : 
	dumpColor ("black"),
	codeSnippet (""),
	nodeTypeName ("")
	{
	}
	
	Node(const int astId, const int traversalOrderNumber, std::string color, std::string code,
	     std::string type) : astId(astId), number(traversalOrderNumber), dumpColor(std::move(color)),
	                         codeSnippet(std::move(code)), nodeTypeName(std::move(type))
	{
	}
};

class DependencyGraph
{
	std::vector<int> criterion_;
	std::unordered_map<int, Node> debugNodeData_;
	std::unordered_map<int, std::vector<int>> edges_;
	std::unordered_map<int, std::vector<int>> inverseEdges_;

public:

	void AddCriterionNode(const int node)
	{
		criterion_.push_back(node);
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

		it = inverseEdges_.find(child);

		if (it == inverseEdges_.end())
		{
			it = inverseEdges_.insert(std::pair<int, std::vector<int>>(child, std::vector<int>())).first;
		}

		it->second.push_back(parent);
	}

	void InsertNodeDataForDebugging(int traversalOrderNumber, const int astId, const std::string& snippet,
	                                const std::string& type, const std::string& color)
	{
		const auto success = debugNodeData_.insert(std::pair<int, Node>(traversalOrderNumber,
		                                                                Node(astId, traversalOrderNumber, color,
		                                                                     snippet, type))).second;

		if (!success)
		{
			llvm::outs() << "DEBUG: Could not add a snippet to the mapping. The snippet:\n" << snippet << "\n";
		}
	}

	void PrintGraphForDebugging() const
	{
		llvm::outs() << "--------------- Dependency graph and its code ---------------\n";

		for (auto it = debugNodeData_.cbegin(); it != debugNodeData_.cend(); ++it)
		{
			llvm::outs() << "Node " << it->first << ":\n" << it->second.codeSnippet << "\n";
		}

		llvm::outs() << "-------------------------------------------------------------\n";
	}

	void DumpDot(const std::string& /*fileName*/) const
	{
		// TODO(Denis): Trim file names (they include full path instead of just the name).
		//auto dotFileName = "visuals/dotDump_" + RemoveFileExtensions(fileName) + ".dot";
		const auto dotFileName = "visuals/dotDump_test.dot";
		auto ofs = std::ofstream(dotFileName);

		ofs << "digraph g {\nforcelabels=true;\nrankdir=TD;\n";

		for (auto it = debugNodeData_.cbegin(); it != debugNodeData_.cend(); ++it)
		{
			ofs << it->first << "[label=\"" << EscapeQuotes(it->second.codeSnippet)
				<< "\", xlabel=\"No. " << it->first << " (" << it->second.astId
				<< "), " << it->second.nodeTypeName << "\", color=\"" << it->second.dumpColor << "\"];\n";
		}

		for (auto it = edges_.cbegin(); it != edges_.cend(); ++it)
		{
			for (auto child : it->second)
			{
				ofs << it->first << " -> " << child << ";\n";
			}
		}

		ofs << "}\n";
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

	std::vector<int> GetParentNodes(const int startingNode)
	{
		const auto it = inverseEdges_.find(startingNode);

		return it != inverseEdges_.end() ? it->second : std::vector<int>();
	}

	bool IsInCriterion(const int node)
	{
		return std::find(criterion_.begin(), criterion_.end(), node) != criterion_.end();
	}
};
#endif
