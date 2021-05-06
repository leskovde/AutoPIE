#ifndef HELPER_NAIVE_H
#define HELPER_NAIVE_H
#pragma once

#include <filesystem>
#include <utility>
#include "../../Common/include/Helper.h"

using namespace Common;

class DependencyGraph;

namespace Naive
{
	//===----------------------------------------------------------------------===//
	//
	/// BitMask helper functions.
	//
	//===----------------------------------------------------------------------===//

	std::string Stringify(BitMask& bitMask);

	bool IsFull(BitMask& bitMask);

	void Increment(BitMask& bitMask);

	std::pair<bool, double> IsValid(BitMask& bitMask, DependencyGraph& dependencies);
}

#endif