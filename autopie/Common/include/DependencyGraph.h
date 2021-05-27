#ifndef DEPENDENCYGRAPH_H
#define DEPENDENCYGRAPH_H
#pragma once

#include <fstream>
#include <queue>
#include <utility>

#include "Helper.h"
#include "Streams.h"

namespace Common
{
}

using namespace Common;

/**
 * Represents a single code unit.\n
 * Specifies the position in the AST, the underlying source code and debug information.
 */
struct Node
{
	int astId{0};
	int number{0};
	int characterCount{0};
	std::string dumpColor;
	std::string codeSnippet;
	std::string nodeTypeName;

	Node() : dumpColor("black"),
	         codeSnippet(""),
	         nodeTypeName("")
	{
	}

	Node(const int astId, const int traversalOrderNumber, const int length, std::string color, std::string code,
	     std::string type) : astId(astId), number(traversalOrderNumber), characterCount(length),
	                         dumpColor(std::move(color)),
	                         codeSnippet(std::move(code)), nodeTypeName(std::move(type))
	{
	}
};

/**
 * Keeps the information about node relationships.\n
 * Specifies the parent to children and child to parent dependencies of code units.\n
 * Keeps nodes found on the error-inducing location.\n
 * Uses additional debug information to dump or print the graph.
 */
class DependencyGraph
{
	int totalCharacters_{0};
	std::vector<int> criterion_;
	std::unordered_map<int, Node> debugNodeData_;
	std::unordered_map<int, std::vector<int>> statementEdges_;
	std::unordered_map<int, std::vector<int>> statementInverseEdges_;
	std::unordered_map<int, std::vector<int>> variableEdges_;
	std::unordered_map<int, std::vector<int>> variableInverseEdges_;
	std::unordered_map<int, std::vector<int>> dependentNodesCache_;

	/**
	 * Recursively searches for all children of a given node in a given unordered map.
	 *
	 * @param startingNode The node whose children should be searched.
	 * @param container The unordered map in which the search should be conducted.
	 */
	static std::vector<int> GetDependentNodesFromContainer(const int startingNode,
	                                                       const std::unordered_map<int, std::vector<int>>& container)
	{
		auto nodeQ = std::queue<int>();
		auto allDependencies = std::vector<int>();

		nodeQ.push(startingNode);

		while (!nodeQ.empty())
		{
			auto currentNode = nodeQ.front();
			nodeQ.pop();

			auto it = container.find(currentNode);

			if (it != container.end())
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

public:

	/**
	 * Adds a node to the error-inducing node container.
	 *
	 * @param node The traversal order number of the error-inducing node.
	 */
	void AddCriterionNode(const int node)
	{
		criterion_.push_back(node);

		debugNodeData_[node].dumpColor = "green";
	}

	/**
	 * Adds a statement dependency edge between two nodes. The edge is bidirectional.
	 *
	 * @param parent The traversal order number of the parent.
	 * @param child The traversal order number of the child.
	 */
	void InsertStatementDependency(const int parent, const int child)
	{
		if (parent == child)
		{
			return;
		}

		auto it = statementEdges_.find(parent);

		if (it == statementEdges_.end())
		{
			it = statementEdges_.insert(std::pair<int, std::vector<int>>(parent, std::vector<int>())).first;
		}

		if (std::find(it->second.begin(), it->second.end(), child) != it->second.end())
		{
			// The dependency has already been made, no need to reintroduce it.
			return;
		}

		it->second.push_back(child);

		it = statementInverseEdges_.find(child);

		if (it == statementInverseEdges_.end())
		{
			it = statementInverseEdges_.insert(std::pair<int, std::vector<int>>(child, std::vector<int>())).first;
		}

		it->second.push_back(parent);
	}

	/**
	 * Adds a variable dependency edge between two nodes. The edge is bidirectional.
	 *
	 * @param parent The traversal order number of the parent.
	 * @param child The traversal order number of the child.
	 */
	void InsertVariableDependency(const int parent, const int child)
	{
		if (parent == child)
		{
			return;
		}

		auto it = variableEdges_.find(parent);

		if (it == variableEdges_.end())
		{
			it = variableEdges_.insert(std::pair<int, std::vector<int>>(parent, std::vector<int>())).first;
		}

		if (std::find(it->second.begin(), it->second.end(), child) != it->second.end())
		{
			// The dependency has already been made, no need to reintroduce it.
			return;
		}

		it->second.push_back(child);

		it = variableInverseEdges_.find(child);

		if (it == variableInverseEdges_.end())
		{
			it = variableInverseEdges_.insert(std::pair<int, std::vector<int>>(child, std::vector<int>())).first;
		}

		it->second.push_back(parent);
	}

	/**
	 * Adds additional data for debugging and pretty printing.
	 *
	 * @param traversalOrderNumber The node number.
	 * @param astId The node's ID as returned by the `node->getId()` method.
	 * @param snippet The node's underlying source code.
	 * @param type The node's type (Stmt, Decl, Expr, ...) in a text form.
	 * @param color The name of the GraphViz color in which the node should be dumped.
	 */
	void InsertNodeDataForDebugging(int traversalOrderNumber, const int astId, const std::string& snippet,
	                                const std::string& type, const std::string& color)
	{
		auto actualColor = color;

		if (debugNodeData_.find(traversalOrderNumber) != debugNodeData_.end())
		{
			Out::Verb() << "DEBUG: A node with the current traversal number already exists.\n";

			actualColor = debugNodeData_[traversalOrderNumber].dumpColor;

			debugNodeData_.erase(traversalOrderNumber);
		}

		const auto success = debugNodeData_.insert(std::pair<int, Node>(traversalOrderNumber,
		                                                                Node(astId, traversalOrderNumber,
		                                                                     snippet.size(), actualColor,
		                                                                     snippet, type))).second;

		if (!success)
		{
			Out::Verb() << "DEBUG: Could not add a snippet to the mapping. The snippet:\n" << snippet << "\n";
		}
	}

	/**
	 * Prints the dependency graph node by node into the console.
	 */
	void PrintGraphForDebugging()
	{
		Out::Verb() << "===------------------- Dependency graph and its code --------------------===\n";

		for (auto it = debugNodeData_.cbegin(); it != debugNodeData_.cend(); ++it)
		{
			Out::Verb() << "Node " << it->first << ":\n" << it->second.codeSnippet << "\n";
		}

		Out::Verb() << "Characters total: " << GetTotalCharacterCount() << "\n";
		Out::Verb() << "===----------------------------------------------------------------------===\n";
	}

	/**
	 * Prints the dependency graph to a GraphViz file.\n
	 * The output is saved to the `visuals` directory, the file name corresponds to the input file's name,
	 * the extension is changed to `.dot`.
	 *
	 * @param fileName The currently processed source file.
	 */
	void DumpDot(const std::string& fileName) const
	{
		if (!std::filesystem::exists(VisualsFolder))
		{
			Out::Verb() << "Creating the 'visuals' directory.\n";

			if (std::filesystem::create_directory(VisualsFolder))
			{
				Out::Verb() << "Directory creation successful.\n";
			}
			else
			{
				Out::Verb() << "Directory creation failed.\n";
			}
		}

		auto ofs = std::ofstream(fileName);

		if (!ofs)
		{
			llvm::errs() << "The GraphViz output file stream could not be opened. Path: " << fileName << "\n";
			return;
		}

		ofs << "digraph g {\nforcelabels=true;\nrankdir=TD;\n";

		for (auto it = debugNodeData_.cbegin(); it != debugNodeData_.cend(); ++it)
		{
			ofs << it->first << "[label=\"" << EscapeQuotes(it->second.codeSnippet)
				<< "\", xlabel=\"No. " << it->first << " (" << it->second.astId
				<< "), " << it->second.nodeTypeName << "\", color=\"" << it->second.dumpColor << "\"];\n";
		}

		for (auto it = statementEdges_.cbegin(); it != statementEdges_.cend(); ++it)
		{
			for (auto child : it->second)
			{
				ofs << it->first << " -> " << child << ";\n";
			}
		}

		for (auto it = variableEdges_.cbegin(); it != variableEdges_.cend(); ++it)
		{
			for (auto child : it->second)
			{
				ofs << it->first << " -> " << child << " [color=maroon];\n";
			}
		}

		ofs << "}\n";
	}

	/**
	 * Searches in a BFS manner for all statement dependent descendants of a given node.
	 *
	 * @param startingNode The node whose descendants are considered.
	 * @return A container of nodes (specified by their traversal order number) that are dependent on
	 * the given node.
	 */
	[[nodiscard]] std::vector<int> GetStatementDependentNodes(const int startingNode) const
	{
		return GetDependentNodesFromContainer(startingNode, statementEdges_);
	}

	/**
	 * Searches in a BFS manner for all variable dependent descendants of a given node.
	 *
	 * @param startingNode The node whose descendants are considered.
	 * @return A container of nodes (specified by their traversal order number) that are dependent on
	 * the given node.
	 */
	[[nodiscard]] std::vector<int> GetVariableDependentNodes(const int startingNode) const
	{
		return GetDependentNodesFromContainer(startingNode, variableEdges_);
	}

	/**
	 * Searches in a BFS manner for all descendants of a given node.
	 * This includes both statement and variable dependencies.\n
	 * Frequently accessed results are cached.
	 *
	 * @param startingNode The node whose descendants are considered.
	 * @return A container of nodes (specified by their traversal order number) that are dependent on
	 * the given node.
	 */
	std::vector<int> GetDependentNodes(const int startingNode)
	{
		if (dependentNodesCache_.find(startingNode) != dependentNodesCache_.end())
		{
			return dependentNodesCache_.find(startingNode)->second;
		}

		auto allDependencies = GetStatementDependentNodes(startingNode);
		auto varDependencies = GetVariableDependentNodes(startingNode);
		allDependencies.insert(allDependencies.end(), std::make_move_iterator(varDependencies.begin()),
		                       std::make_move_iterator(varDependencies.end()));

		dependentNodesCache_.insert(std::pair<int, std::vector<int>>(startingNode, allDependencies));

		return allDependencies;
	}

	/**
	 * Searches for all immediate parent nodes.
	 *
	 * @param startingNode The child whose parents will be searched for.
	 * @return A container of nodes (specified by their traversal order number) that are
	 * the direct parents of the given node. 
	 */
	std::vector<int> GetParentNodes(const int startingNode)
	{
		const auto it = statementInverseEdges_.find(startingNode);

		return it != statementInverseEdges_.end() ? it->second : std::vector<int>();
	}

	/**
	 * Determines whether a node is on the error-inducing location.
	 *
	 * @param node The node to be checked.
	 * @return True if the node's location is the error-inducing file and line, false otherwise.
	 */
	bool IsInCriterion(const int node)
	{
		return std::find(criterion_.begin(), criterion_.end(), node) != criterion_.end();
	}

	/**
	 * Getter for the n-th node in the debug data container.
	 *
	 * @param node The traversal order number of the node.
	 * @return The `Node` object for the node specified by the given number.
	 */
	Node GetNodeInfo(const int node)
	{
		return debugNodeData_[node];
	}

	/**
	 * Getter for the file's (graph's) total number of characters.
	 * During the method's first call, the total character count is calculated
	 * and the count of each node is corrected.
	 *
	 * @return The number of characters in all snippets summed.
	 */
	int GetTotalCharacterCount()
	{
		if (totalCharacters_ == 0)
		{
			std::unordered_map<int, int> correctedCounts;

			for (auto it = statementEdges_.begin(); it != statementEdges_.end(); ++it)
			{
				auto currentCount = GetNodeInfo(it->first).characterCount;

				for (auto dependency : it->second)
				{
					currentCount -= GetNodeInfo(dependency).characterCount;
				}

				correctedCounts[it->first] = currentCount;
			}

			for (auto it = correctedCounts.begin(); it != correctedCounts.end(); ++it)
			{
				debugNodeData_[it->first].characterCount = it->second;
			}

			for (auto it = debugNodeData_.begin(); it != debugNodeData_.end(); ++it)
			{
				totalCharacters_ += it->second.characterCount;
			}
		}

		return totalCharacters_;
	}
};
#endif
