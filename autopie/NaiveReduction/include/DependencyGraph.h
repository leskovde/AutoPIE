#ifndef DEPENDENCYGRAPH_H
#define DEPENDENCYGRAPH_H
#pragma once

#include <fstream>
#include <queue>
#include <utility>

#include "Helper.h"
#include "Streams.h"

/**
 * Represents a single code unit.\n
 * Specifies the position in the AST, the underlying source code and debug information.
 */
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

/**
 * Keeps the information about node relationships.\n
 * Specifies the parent to children and child to parent dependencies of code units.\n
 * Keeps nodes found on the error-inducing location.\n
 * Uses additional debug information to dump or print the graph.
 */
class DependencyGraph
{
	std::vector<int> criterion_;
	std::unordered_map<int, Node> debugNodeData_;
	std::unordered_map<int, std::vector<int>> edges_;
	std::unordered_map<int, std::vector<int>> inverseEdges_;

public:

	/**
	 * Adds a node to the error-inducing node container.
	 *
	 * @param node The traversal order number of the error-inducing node.
	 */
	void AddCriterionNode(const int node)
	{
		criterion_.push_back(node);
	}

	/**
	 * Adds an edge between two nodes. The edge is bidirectional.
	 *
	 * @param parent The traversal order number of the parent.
	 * @param child The traversal order number of the child.
	 */
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
		const auto success = debugNodeData_.insert(std::pair<int, Node>(traversalOrderNumber,
		                                                                Node(astId, traversalOrderNumber, color,
		                                                                     snippet, type))).second;

		if (!success)
		{
			out::Verb() << "DEBUG: Could not add a snippet to the mapping. The snippet:\n" << snippet << "\n";
		}
	}

	/**
	 * Prints the dependency graph node by node into the console.
	 */
	void PrintGraphForDebugging() const
	{
		out::Verb() << "===------------------- Dependency graph and its code --------------------===\n";

		for (auto it = debugNodeData_.cbegin(); it != debugNodeData_.cend(); ++it)
		{
			out::Verb() << "Node " << it->first << ":\n" << it->second.codeSnippet << "\n";
		}

		out::Verb() << "===----------------------------------------------------------------------===\n";
	}

	/**
	 * Prints the dependency graph to a GraphViz file.\n
	 * The output is saved to the `visuals` directory, the file name corresponds to the input file's name,
	 * the extension is changed to `.dot`.
	 *
	 * @param fileName The currently processed source file.
	 */
	void DumpDot(const std::string& /*fileName*/) const
	{
		// TODO(Denis): Trim file names (they include full path instead of just the name).
		const auto dotFileName = VisualsFolder + std::string("dotDump_test.dot");
		auto ofs = std::ofstream(dotFileName);

		if (!ofs)
		{
			llvm::errs() << "The GraphViz output file stream could not be opened. Path: " << dotFileName << "\n";
			return;
		}
		
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

	/**
	 * Searches in a BFS manner for all descendants of a given node.
	 *
	 * @param startingNode The node whose descendants are considered.
	 * @return A container of nodes (specified by their traversal order number) that are dependent on
	 * the given node.
	 */
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

	/**
	 * Searches for all immediate parent nodes.
	 *
	 * @param startingNode The child whose parents will be searched for.
	 * @return A container of nodes (specified by their traversal order number) that are
	 * the direct parents of the given node. 
	 */
	std::vector<int> GetParentNodes(const int startingNode)
	{
		const auto it = inverseEdges_.find(startingNode);

		return it != inverseEdges_.end() ? it->second : std::vector<int>();
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
};
#endif
