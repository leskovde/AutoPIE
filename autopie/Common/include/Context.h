#ifndef CONTEXT_H
#define CONTEXT_H
#pragma once

#include "Helper.h"
#include "Streams.h"

using EpochRanges = std::map<double, std::vector<BitMask>>;

namespace Naive
{
	/**
	 * Keeps the data concerned with iterative deepening, such as epoch count and bit masks for each epoch.
	 */
	struct IterativeDeepeningContext
	{
		const int epochCount;
		const double epochStep;
		EpochRanges bitMasks;

		explicit IterativeDeepeningContext(const int epochs) : epochCount(epochs),
		                                                       epochStep(
			                                                       static_cast<double>(ReductionRatio) / epochCount)
		{
		}
	};
} // namespace Naive

namespace Delta
{
	struct DeltaAlgorithmContext
	{
		int latestCodeUnitCount{0};
	};
} // namespace Delta

struct Statistics
{
	double expectedIterations = 0;
	size_t totalIterations = 0;
	size_t inputSizeInBytes = 0;
	size_t outputSizeInBytes = 0;
	int exitCode = EXIT_FAILURE;

	Statistics()
	= default;

	explicit Statistics(const std::string& inputFile)
	{
		inputSizeInBytes = file_size(std::filesystem::path(inputFile));
	}

	void Finalize(const std::string& outputFile)
	{
		outputSizeInBytes = file_size(std::filesystem::path(outputFile));
	}
};

/**
 * Serves as a container for all publicly available global information.
 * Currently includes the parsed input.
 */
class GlobalContext
{
	GlobalContext() : parsedInput(InputData("", Location("", 0), 0.0, false)), deepeningContext(1)
	{
		Out::Verb() << "DEBUG: GlobalContext - New default constructor call.\n";
	}

public:

	// Variant generation properties.
	Statistics stats;
	int currentEpoch{0};
	InputData parsedInput;
	Delta::DeltaAlgorithmContext deltaContext;
	Naive::IterativeDeepeningContext deepeningContext;
	clang::Language language{clang::Language::Unknown};
	std::unordered_map<size_t, std::vector<size_t>> variantAdjustedErrorLocations;

	GlobalContext(InputData& input, const std::string& inputFile, const int epochs) : stats(inputFile),
	                                                                                  parsedInput(input),
	                                                                                  deepeningContext(epochs)
	{
		Out::Verb() << "DEBUG: GlobalContext - New non-default constructor call.\n";
	}
};

#endif
