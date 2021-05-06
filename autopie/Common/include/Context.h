#ifndef CONTEXT_H
#define CONTEXT_H
#pragma once

#include "Helper.h"
#include "Streams.h"

namespace Delta
{
	/**
	 * Keeps the data concerned with iterative deepening, such as epoch count and bit masks for each epoch.
	 */
	struct IterativeDeepeningContext
	{
		const int epochCount;
		const double epochStep;
		std::map<double, std::vector<BitMask>> bitMasks;

		explicit IterativeDeepeningContext(const int epochs) : epochCount(epochs),
		                                                       epochStep(static_cast<double>(ReductionRatio) / epochCount)
		{
		}
	};
}

/**
 * Serves as a container for all publicly available global information.
 * Currently includes the parsed input.
 */
class GlobalContext
{
	GlobalContext() : parsedInput(InputData("", Location("", 0), 0.0, false)), deepeningContext(1)
	{
		out::Verb() << "DEBUG: GlobalContext - New default constructor call.\n";
	}

public:

	// Variant generation properties.
	int currentEpoch{ 0 };
	InputData parsedInput;
	Delta::IterativeDeepeningContext deepeningContext;
	clang::Language language{ clang::Language::Unknown };
	std::unordered_map<int, int> variantAdjustedErrorLocation;

	GlobalContext(InputData& input, const std::string& /*source*/, const int epochs) : parsedInput(input),
		deepeningContext(epochs)
	{
		out::Verb() << "DEBUG: GlobalContext - New non-default constructor call.\n";
	}
};

#endif