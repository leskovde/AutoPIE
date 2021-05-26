#ifndef CONSUMERS_NAIVE_H
#define CONSUMERS_NAIVE_H
#pragma once

#include <clang/AST/ASTConsumer.h>

#include <future>
#include <utility>

#include "../../Common/include/Context.h"
#include "../../Common/include/DependencyGraph.h"
#include "../../Common/include/Visitors.h"
#include "../../Common/include/Streams.h"
#include "../../Common/include/Consumers.h"
#include "../../Common/include/Helper.h"

namespace Naive
{
	/**
	 * Unifies other consumers and uses them to describe the variant generation logic.\n
	 * Single `HandleTranslationUnit` generates all source code variants.
	 */
	class VariantGeneratingConsumer final : public clang::ASTConsumer
	{
		DependencyMappingASTConsumer mappingConsumer_;
		VariantPrintingASTConsumer printingConsumer_;
		GlobalContext& globalContext_;

	public:
		VariantGeneratingConsumer(clang::CompilerInstance* ci, GlobalContext& context) : mappingConsumer_(ci, context),
			printingConsumer_(ci, context.parsedInput.errorLocation.lineNumber),
			globalContext_(context)
		{
		}

		void GenerateVariantsForABin(clang::ASTContext& context, const std::vector<std::vector<bool>>& bitMasks) const
		{
			auto variantsCount = 0;
			for (auto& bitMask : bitMasks)
			{
				variantsCount++;
				globalContext_.stats.totalIterations++;

				if (variantsCount % 50 == 0)
				{
					Out::All() << "Done " << variantsCount << " variants.\n";
				}

				Out::Verb() << "DEBUG: Processing valid bitmask " << Stringify(bitMask) << "\n";

				try
				{
					auto fileName = TempFolder + std::to_string(variantsCount) + "_" + GetFileName(globalContext_.parsedInput.errorLocation.filePath) + LanguageToExtension(globalContext_.language);
					printingConsumer_.HandleTranslationUnit(context, fileName, bitMask);

					globalContext_.variantAdjustedErrorLocations[variantsCount] = printingConsumer_.GetAdjustedErrorLines();
				}
				catch (...)
				{
					Out::All() << "Could not process iteration no. " << variantsCount << " due to an internal exception.\n";
				}
			}

			Out::All() << "Finished. Done " << variantsCount << " variants.\n";
		}

		EpochRanges GetValidBitMasksInRange(const Unsigned startingPoint, const Unsigned numberOfVariants, const int numberOfCodeUnits, DependencyGraph* dependencies) const
		{
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
			for (Unsigned i = 0; i < numberOfVariants; i++)
			{
				Increment(bitMask);

				const auto validation = IsValid(bitMask, *dependencies);

				if (validation.first)
				{
					auto it = bins.upper_bound(validation.second);
					it->second.push_back(bitMask);
				}
			}

			return bins;
		}
		
		void PartitionVariantsIntoBins(const int numberOfCodeUnits, DependencyGraph& dependencies) const
		{
			Out::All() << "Binning variants...\n";

			const auto totalNumberOfVariants = static_cast<Unsigned>(1) << numberOfCodeUnits;

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
					std::pair<double, std::vector<BitMask>>((i + 1) * globalContext_.deepeningContext.epochStep,
					                                        std::vector<BitMask>()));
			}

			// Add the last range (of invalid bit masks).
			globalContext_.deepeningContext.bitMasks.insert(std::pair<double, std::vector<BitMask>>(1.0, std::vector<BitMask>()));
			globalContext_.deepeningContext.bitMasks.insert(std::pair<double, std::vector<BitMask>>(INFINITY, std::vector<BitMask>()));

			const auto originalVariant = BitMask(numberOfCodeUnits, true);
			globalContext_.deepeningContext.bitMasks.at(1.0).push_back(originalVariant);

			// The thread count must be specified in code, since it must have the const qualifier.
			const auto threadCount = 12;
			Unsigned ranges[threadCount];

			// Determine the number of variants for each thread.
			for (auto i = 0; i < threadCount; i++)
			{
				ranges[i] = (totalNumberOfVariants - 1) / threadCount;
			}

			for (Unsigned i = 0; i < (totalNumberOfVariants - 1) % threadCount; i++)
			{
				ranges[i]++;
			}

			auto futures = std::vector<std::future<EpochRanges>>();
			
			// Launch all threads.
			for (auto i = 0; i < threadCount; i++)
			{
				const auto numberOfVariants = ranges[i];
				Unsigned startingPoint = 0;
				
				// Determine the starting point of each thread by summing up the previous counts.
				if (i > 0)
				{
					ranges[i] += ranges[i - 1];
					startingPoint = ranges[i - 1];
				}

				futures.emplace_back(std::async(std::launch::async, &VariantGeneratingConsumer::GetValidBitMasksInRange, this, startingPoint, numberOfVariants, numberOfCodeUnits, &dependencies));
			}

			auto results = std::vector<EpochRanges>();

			for (auto& future : futures)
			{
				results.push_back(future.get());
			}
			
			// Distribute bit masks into ranges.
			for (auto& result : results)
			{
				MergeVectorMaps(result, globalContext_.deepeningContext.bitMasks);
			}
		}

		/**
		 * Runs two consumer steps to generate all possible variants.\n
		 * Firstly, a mapping consumer is called to analyze AST nodes and create a traversal order.\n
		 * Secondly, a bitmask based on the number of found code units is created.
		 * Each bit in the bitmask represents a node based on the traversal order.
		 * Setting a bit to true translates to the corresponding node being present in the output.
		 * Setting a bit to false results in the deletion of the corresponding node.\n
		 * Lastly, all possible bitmask variants are generated and the variant printing consumer is called
		 * for each bitmask variant.
		 *
		 * @param context The AST context.
		 */
		void HandleTranslationUnit(clang::ASTContext& context) override
		{
			mappingConsumer_.HandleTranslationUnit(context);
			const auto numberOfCodeUnits = mappingConsumer_.GetCodeUnitsCount();

			globalContext_.variantAdjustedErrorLocations.clear();
			printingConsumer_.SetData(mappingConsumer_.GetSkippedNodes(), mappingConsumer_.GetDependencyGraph(), mappingConsumer_.GetPotentialErrorLines());

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
			
			for (auto i = 0; i < globalContext_.deepeningContext.epochCount; i++)
			{			
				auto& bitMasks = globalContext_.deepeningContext.bitMasks.lower_bound((i+ 1) * globalContext_.deepeningContext.epochStep - globalContext_.deepeningContext.epochStep / 2)->second;

				GenerateVariantsForABin(context, bitMasks);

				if (ValidateResults(globalContext_))
				{
					globalContext_.stats.exitCode = EXIT_SUCCESS;
					return;
				}

				Out::All() << "Epoch " << i + 1 << " out of " << globalContext_.deepeningContext.epochCount << ": A smaller program variant could not be found.\n";
				ClearTempDirectory();
			}

			Out::All() << "A reduced variant could not be found. If you've manually set the `--ratio` option, consider trying a greater value.\n";
			
			globalContext_.stats.exitCode = EXIT_FAILURE;
		}
	};
}

#endif
