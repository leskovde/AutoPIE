#ifndef CONSUMERS_DELTA_H
#define CONSUMERS_DELTA_H
#pragma once

#include <clang/AST/ASTConsumer.h>

#include "../../Common/include/Consumers.h"
#include "../../Common/include/Context.h"
#include "../../Common/include/DependencyGraph.h"
#include "../../Common/include/Helper.h"
#include "../../Common/include/Streams.h"

namespace Delta
{
	/**
	 * Unifies other consumers and uses them to describe the Delta debugging logic.\n
	 * Single `HandleTranslationUnit` manages one run of the Delta debugging algorithm.
	 */
	class DeltaDebuggingConsumer final : public clang::ASTConsumer
	{
		DependencyMappingASTConsumer mappingConsumer_;
		VariantPrintingASTConsumer printingConsumer_;
		int iteration_;
		int partitionCount_;
		const std::string fileName_;
		GlobalContext& globalContext_;
		DeltaIterationResults& result_;

		/**
		 * Validates the current bit mask by generating source code, compiling it and
		 * executing it.
		 *
		 * @param context ASTContext of the current traversal.
		 * @param bitmask The bit mask on which the source code variant should be based.
		 * @param dependencyGraph The graph for heuristics and printing-safety.
		 * @return True if the variant represented by the given bit mask was correct,
		 * false otherwise.
		 */
		bool IsFailureInducingSubset(clang::ASTContext& context, const BitMask& bitmask,
		                             DependencyGraph& dependencyGraph) const
		{
			// Check whether the bit mask is worth generating into source code.
			if (IsValid(bitmask, dependencyGraph, false).first)
			{
				globalContext_.stats.totalIterations++;

				try
				{
					if (std::filesystem::exists(fileName_))
					{
						std::filesystem::remove(fileName_);
					}

					// Convert the bit mask into source code, update the adjusted locations.
					printingConsumer_.HandleTranslationUnit(context, fileName_, bitmask);
					globalContext_.variantAdjustedErrorLocations[iteration_] = printingConsumer_.
						GetAdjustedErrorLines();

					// Compile and execute the generated source code.
					if (ValidateVariant(globalContext_, std::filesystem::directory_entry(fileName_)))
					{
						Out::All() << "Iteration " << iteration_ << ": smaller subset found.\n";
						return true;
					}
				}
				catch (...)
				{
					Out::All() << "Could not process a subset due to an internal exception.\n";
				}
			}

			return false;
		}

	public:
		DeltaDebuggingConsumer(clang::CompilerInstance* ci, GlobalContext& context, const int iteration,
		                       const int partitionCount, DeltaIterationResults& result) : mappingConsumer_(ci, context,
		                                                                                                   iteration),
		                                                                                  printingConsumer_(
			                                                                                  ci, context
			                                                                                      .parsedInput.
			                                                                                      errorLocation.
			                                                                                      lineNumber),
		                                                                                  iteration_(iteration),
		                                                                                  partitionCount_(
			                                                                                  partitionCount),
		                                                                                  fileName_(
			                                                                                  TempFolder + std::
			                                                                                  to_string(iteration_) +
			                                                                                  "_" + GetFileName(
				                                                                                  context.parsedInput.
				                                                                                          errorLocation.
				                                                                                          filePath) +
			                                                                                  LanguageToExtension(
				                                                                                  context.language)),
		                                                                                  globalContext_(context),
		                                                                                  result_(result)

		{
		}

		/**
		 * Runs the iteration body of the minimizing Delta debugging algorithm.\n
		 * Performs the following steps:\n
		 * 1. Split into a container of equal sized bins.\n
		 * 2. Get a container of complements.\n
		 * 3. Loop over the first container and test each bin (generate and execute).\n
		 * 4. If a variant fails (i.e., the desired outcome), set granularity to 2 and the file
		 * to that variant.\n
		 * 5. Loop over the second container and test analogically.\n
		 * 6. If a variant fails, decrement granularity and set the file to that variant \n
		 * 7. If nothing fails, multiply granularity by 2.\n
		 * Granularity is set elsewhere, the function only propagates the result of the iteration.
		 * @param context The AST context.
		 */
		void HandleTranslationUnit(clang::ASTContext& context) override
		{
			// Map the current file - must be done, since the file changes in each iteration.
			mappingConsumer_.HandleTranslationUnit(context);
			const auto numberOfCodeUnits = mappingConsumer_.GetCodeUnitsCount();

			globalContext_.deltaContext.latestCodeUnitCount = numberOfCodeUnits;
			globalContext_.variantAdjustedErrorLocations.clear();

			printingConsumer_.SetData(mappingConsumer_.GetSkippedNodes(), mappingConsumer_.GetDependencyGraph(),
			                          mappingConsumer_.GetPotentialErrorLines());

			auto dependencies = mappingConsumer_.GetDependencyGraph();

			Out::Verb() << "Current iteration: " << iteration_ << ".\n";
			Out::Verb() << "Current code unit count: " << numberOfCodeUnits << ".\n";
			Out::Verb() << "Current partition count: " << partitionCount_ << ".\n";

			if (partitionCount_ > numberOfCodeUnits)
			{
				// Cannot be split further.
				Out::Verb() << "The current test case cannot be split further.\n";

				result_ = DeltaIterationResults::Unsplitable;
				return;
			}

			std::vector<BitMask> partitions;
			std::vector<BitMask> complements;
			const auto partitionSize = numberOfCodeUnits / partitionCount_;

			// Split into `n` partition and their complements.
			Out::Verb() << "Splitting " << numberOfCodeUnits << " code units into " << partitionCount_
				<< " partitions of size " << partitionSize << " units...\n";

			// Create even-sized splittings.
			std::vector<int> ranges(partitionCount_);

			for (auto i = 0; i < partitionCount_; i++)
			{
				ranges[i] = partitionSize;
			}

			for (auto i = 0; i < numberOfCodeUnits % partitionCount_; i++)
			{
				ranges[i]++;
			}

			// Assign bits into partitions.
			auto sum = 0;
			for (auto i = 0; i < partitionCount_; i++)
			{
				auto partition = BitMask(numberOfCodeUnits);
				auto complement = BitMask(numberOfCodeUnits);

				// Determine whether each code unit belongs to the current partition.
				for (auto j = 0; j < numberOfCodeUnits; j++)
				{
					if (sum <= j && j < sum + ranges[i])
					{
						partition[j] = true;
						complement[j] = false;
					}
					else
					{
						partition[j] = false;
						complement[j] = true;
					}
				}

				sum += ranges[i];

				partitions.emplace_back(partition);
				complements.emplace_back(complement);
			}

			Out::Verb() << "Splitting done.\n";
			Out::Verb() << "Validating " << partitions.size() << " partitions...\n";

			// Iterate over all partitions - the small kind of input.
			for (auto& partition : partitions)
			{
				if (IsFailureInducingSubset(context, partition, dependencies))
				{
					result_ = DeltaIterationResults::FailingPartition;
					return;
				}
			}

			Out::Verb() << "Validating " << complements.size() << " complements...\n";

			// Iterate over all complements - the larger kind of input.
			for (auto& complement : complements)
			{
				if (IsFailureInducingSubset(context, complement, dependencies))
				{
					result_ = DeltaIterationResults::FailingComplement;
					return;
				}
			}

			// The iteration didn't find any valid subsets.
			Out::Verb() << "Iteration " << iteration_ << ": smaller subset not found.\n";
			result_ = DeltaIterationResults::Passing;
		}
	};
} // namespace Delta

#endif
