#ifndef CONSUMERS_NAIVE_H
#define CONSUMERS_NAIVE_H
#pragma once

#include <clang/AST/ASTConsumer.h>

#include <future>
#include <utility>

#include "../../Common/include/Consumers.h"
#include "../../Common/include/Context.h"
#include "../../Common/include/DependencyGraph.h"
#include "../../Common/include/Helper.h"
#include "../../Common/include/Streams.h"
#include "../../Common/include/Visitors.h"

namespace Naive
{
	inline std::mutex streamMutex; ///< Locks the output stream in order for messages to appear correctly.

	/**
	 * Unifies other consumers and uses them to describe the naive variant-generating logic.\n
	 * Single `HandleTranslationUnit` generates all source code variants and performs the validation.\n
	 * Note that the validation is done in partitions - bins.
	 */
	class VariantGeneratingConsumer final : public clang::ASTConsumer
	{
		DependencyMappingASTConsumer mappingConsumer_;
		VariantPrintingASTConsumer printingConsumer_;
		GlobalContext& globalContext_;

	public:
		VariantGeneratingConsumer(clang::CompilerInstance* ci, GlobalContext& context) : mappingConsumer_(ci, context),
		                                                                                 printingConsumer_(
			                                                                                 ci, context
			                                                                                     .parsedInput.
			                                                                                     errorLocation.
			                                                                                     lineNumber),
		                                                                                 globalContext_(context)
		{
		}

		/**
		 * Generates all source code variants for each bit mask in a container of bit masks.\n
		 * The source code is saved to the temporary directory under the name of the current iteration.\n
		 * Adjusted line numbers are extracted while generating and saved in a map for future use.
		 *
		 * @param context ASTContext of the current traversal.
		 * @param bitMasks A container of bit masks to be iterated, for which variants will be generated.
		 */
		void GenerateVariantsForABin(clang::ASTContext& context, const std::vector<BitMask>& bitMasks) const
		{
			auto variantsCount = 0;
			for (auto& bitMask : bitMasks)
			{
				variantsCount++;
				globalContext_.stats.totalIterations++;

				// Print the progress.
				if (variantsCount % 100 == 0)
				{
					Out::All() << "Done " << variantsCount << " variants.\n";
				}

				Out::Verb() << "Processing valid bitmask " << Stringify(bitMask) << "\n";

				// If we have done something incorrectly - wrong Rewriter buffer overrides,
				// null nodes dereferences, ..., LibTooling will thrown an appropriate error.
				// The error shouldn't stop us from generating other variants, though.
				try
				{
					auto fileName = TempFolder + std::to_string(variantsCount) + "_" + GetFileName(
						globalContext_.parsedInput.errorLocation.filePath) + LanguageToExtension(
						globalContext_.language);
					printingConsumer_.HandleTranslationUnit(context, fileName, bitMask);

					globalContext_.variantAdjustedErrorLocations[variantsCount] = printingConsumer_.
						GetAdjustedErrorLines();
				}
				catch (...)
				{
					Out::All() << "Could not process iteration no. " << variantsCount <<
						" due to an internal exception.\n";
				}
			}

			Out::All() << "Finished. Done " << variantsCount << " variants.\n";
		}

		/**
		 * A worker function for parallel runs.\n
		 * Given a starting bit mask and a number of iterations, the function iterates over all
		 * upcoming bit masks (by incrementing) and checks their validity using bit mask heuristics.
		 *
		 * @param startingPoint The number representing the starting bit mask - it will be converted
		 * into a bit mask later.
		 * @param numberOfVariants The number of iterations - new bit masks to be checked.
		 * @param numberOfCodeUnits The size of the bit mask.
		 * @param dependencies A copy of the dependency graph.
		 * A copy allows us to avoid locking the graph when inserting cache entries.
		 * @param id The number of the thread used for printing the progress.
		 * @return All processed bit masks separated into bins - a map of bit mask containers accessible
		 * by a given size ratio.
		 */
		[[nodiscard]] EpochRanges GetValidBitMasksInRange(const size_t startingPoint, const size_t numberOfVariants,
		                                                  const int numberOfCodeUnits, DependencyGraph dependencies,
		                                                  const int id) const
		{
			{
				std::lock_guard<std::mutex> lock(streamMutex);
				Out::All() << "Thread #" << id << " started.\n";
			}

			// Create ranges for each epoch.
			EpochRanges bins;

			for (auto i = 0; i < globalContext_.deepeningContext.epochCount; i++)
			{
				bins.insert(
					std::pair<double, std::vector<BitMask>>((i + 1) * globalContext_.deepeningContext.epochStep,
					                                        std::vector<BitMask>()));
			}

			// Add the last range (of invalid bit masks).
			bins.insert(std::pair<double, std::vector<BitMask>>(1.0, std::vector<BitMask>()));
			bins.insert(std::pair<double, std::vector<BitMask>>(INFINITY, std::vector<BitMask>()));

			auto bitMask = BitMask(numberOfCodeUnits);
			InitializeBitMask(bitMask, startingPoint);

			// Iterate over the given range and assign valid bit masks into bins.
			for (size_t i = 0; i < numberOfVariants; i++)
			{
				Increment(bitMask);

				const auto validation = IsValid(bitMask, dependencies);

				if (validation.first)
				{
					auto it = bins.upper_bound(validation.second);
					it->second.push_back(bitMask);
				}
			}

			{
				std::lock_guard<std::mutex> lock(streamMutex);
				Out::All() << "Thread #" << id << " finished.\n";
			}

			return bins;
		}

		/**
		 * Launches worker threads to validate all possible bit masks.\n
		 * Creates ranges for the workers and assigns them to each thread.\n
		 * Merges all results.
		 *
		 * @param numberOfCodeUnits The size of each bit mask.
		 * @param dependencies The dependency graph for heuristics.
		 */
		void PartitionVariantsIntoBins(const int numberOfCodeUnits, DependencyGraph& dependencies) const
		{
			Out::All() << "Binning variants...\n";

			const auto totalNumberOfVariants = static_cast<size_t>(1) << numberOfCodeUnits;

			if (totalNumberOfVariants == 0)
			{
				std::cerr << "The expected number of variants is greater than supported data types."
					<< "It is not wise to run the algorithm on such a large input. Exiting...\n";
				return;
			}

			// Create ranges for each epoch.
			for (auto i = 0; i < globalContext_.deepeningContext.epochCount; i++)
			{
				globalContext_.deepeningContext.bitMasks.insert(
					std::pair<double, std::vector<BitMask>>(
						(i + 1) * globalContext_.deepeningContext.epochStep,
						std::vector<BitMask>()));
			}

			const auto originalVariant = BitMask(numberOfCodeUnits, true);
			globalContext_.deepeningContext.bitMasks.at(
				               globalContext_.deepeningContext.epochCount * globalContext_.deepeningContext.epochStep).
			               push_back(originalVariant);

			// Add the last range (of invalid bit masks).
			globalContext_.deepeningContext.bitMasks.insert(
				std::pair<double, std::vector<BitMask>>(1.0, std::vector<BitMask>()));
			globalContext_.deepeningContext.bitMasks.insert(
				std::pair<double, std::vector<BitMask>>(INFINITY, std::vector<BitMask>()));

			// The thread count must be specified in code, since it must have the const qualifier.
			const auto threadCount = 12;
			size_t ranges[threadCount];

			// Determine the number of variants for each thread.
			for (auto& range : ranges)
			{
				range = (totalNumberOfVariants - 1) / threadCount;
			}

			for (size_t i = 0; i < (totalNumberOfVariants - 1) % threadCount; i++)
			{
				ranges[i]++;
			}

			auto futures = std::vector<std::future<EpochRanges>>();

			// Launch all threads.
			for (auto i = 0; i < threadCount; i++)
			{
				const auto numberOfVariants = ranges[i];
				size_t startingPoint = 0;

				// Determine the starting point of each thread by summing up the previous counts.
				if (i > 0)
				{
					ranges[i] += ranges[i - 1];
					startingPoint = ranges[i - 1];
				}

				futures.emplace_back(std::async(std::launch::async, &VariantGeneratingConsumer::GetValidBitMasksInRange,
				                                this, startingPoint, numberOfVariants, numberOfCodeUnits, dependencies,
				                                i));
			}

			auto results = std::vector<EpochRanges>();

			// Collect the results and wait for all workers.
			for (auto& future : futures)
			{
				results.push_back(future.get());
			}

			// Distribute bit masks into ranges.
			for (auto& result : results)
			{
				MergeVectorMaps(result, globalContext_.deepeningContext.bitMasks);
			}

			Out::All() << "Binning done.\n";
		}

		/**
		 * Runs two consumer steps to generate all possible variants.\n
		 * Firstly, a mapping consumer is called to analyze AST nodes and create a traversal order.\n
		 * Secondly, a bitmask based on the number of found code units is created.
		 * Each bit in the bitmask represents a node based on the traversal order.
		 * Setting a bit to true translates to the corresponding node being present in the output.
		 * Setting a bit to false results in the deletion of the corresponding node.\n
		 * Lastly, all possible bitmask variants are generated, converted to source code, and validated.
		 *
		 * @param context The AST context.
		 */
		void HandleTranslationUnit(clang::ASTContext& context) override
		{
			mappingConsumer_.HandleTranslationUnit(context);
			const auto numberOfCodeUnits = mappingConsumer_.GetCodeUnitsCount();

			globalContext_.variantAdjustedErrorLocations.clear();
			printingConsumer_.SetData(mappingConsumer_.GetSkippedNodes(), mappingConsumer_.GetDependencyGraph(),
			                          mappingConsumer_.GetPotentialErrorLines());

			auto dependencies = mappingConsumer_.GetDependencyGraph();

			globalContext_.stats.expectedIterations = pow(2, numberOfCodeUnits);

			// The first epoch requires special handling.
			// All valid bitmask variants should be iterated and separated into bins based on their size.
			// The bins are then iterated in their respective epochs.
			PartitionVariantsIntoBins(numberOfCodeUnits, dependencies);

			if (globalContext_.deepeningContext.bitMasks.empty())
			{
				globalContext_.stats.exitCode = EXIT_FAILURE;
				return;
			}

			// Process in epochs, generating only a portion of all variants.
			for (auto i = 0; i < globalContext_.deepeningContext.epochCount; i++)
			{
				auto& bitMasks = globalContext_.deepeningContext.bitMasks.lower_bound(
					(i + 1) * globalContext_.deepeningContext.epochStep - globalContext_
					                                                      .deepeningContext
					                                                      .epochStep /
					2)->second;

				GenerateVariantsForABin(context, bitMasks);

				if (ValidateResults(globalContext_))
				{
					globalContext_.stats.exitCode = EXIT_SUCCESS;
					return;
				}

				Out::All() << "Epoch " << i + 1 << " out of " << globalContext_.deepeningContext.epochCount <<
					": A smaller program variant could not be found.\n";
				ClearTempDirectory();
			}

			Out::All() <<
				"A reduced variant could not be found. If you've manually set the `--ratio` option, consider trying a greater value.\n";

			globalContext_.stats.exitCode = EXIT_FAILURE;
		}
	};
} // namespace Naive

#endif
