#ifndef CONSUMERS_DELTA_H
#define CONSUMERS_DELTA_H
#pragma once

#include <clang/AST/ASTConsumer.h>

#include "Visitors.h"
#include "../../Common/include/Context.h"
#include "../../Common/include/DependencyGraph.h"
#include "../../Common/include/Streams.h"
#include "../../Common/include/Consumers.h"
#include "../../Common/include/Helper.h"

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

		bool IsFailureInducingSubset(clang::ASTContext& context, const BitMask& bitmask, DependencyGraph& dependencyGraph) const
		{
			if (IsValid(bitmask, dependencyGraph, false).first)
			{
				globalContext_.stats.totalIterations++;

				try
				{
					if (std::filesystem::exists(fileName_))
					{
						std::filesystem::remove(fileName_);
					}
					
					printingConsumer_.HandleTranslationUnit(context, fileName_, bitmask);
					globalContext_.variantAdjustedErrorLocations[iteration_] = printingConsumer_.GetAdjustedErrorLines();

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
		                       const int partitionCount, DeltaIterationResults& result) : mappingConsumer_(ci, context, iteration),
			printingConsumer_(ci, context.parsedInput.errorLocation.lineNumber),
			iteration_(iteration), partitionCount_(partitionCount),
			fileName_(
				TempFolder + std::to_string(iteration_) + "_" + GetFileName(
					context.parsedInput.errorLocation.filePath) + LanguageToExtension(context.language)), globalContext_(context),
			result_(result)
		
		{
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

			globalContext_.deltaContext.latestCodeUnitCount = numberOfCodeUnits;
			globalContext_.variantAdjustedErrorLocations.clear();

			printingConsumer_.SetData(mappingConsumer_.GetSkippedNodes(), mappingConsumer_.GetDependencyGraph(), mappingConsumer_.GetPotentialErrorLines());

			auto dependencies = mappingConsumer_.GetDependencyGraph();
			
			// 1. Split into a container of equal sized bins.
			// 2. Get a container of complements.
			// 3. Loop over the first container and test each bin (generate variant and execute).
			// 4. If a variant fails, set n to 2 and the file to that variant.
			// 5. Loop over the second container and test --||--.
			// 6. If a variant fails, decrement n and set the file to that variant/
			// 7. If nothing fails, set n to 2 *n.

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
			
			for (auto& partition : partitions)
			{
				if (IsFailureInducingSubset(context, partition, dependencies))
				{
					result_ = DeltaIterationResults::FailingPartition;
					return;
				}
			}

			Out::Verb() << "Validating " << complements.size() << " complements...\n";
			
			for (auto& complement : complements)
			{
				if (IsFailureInducingSubset(context, complement, dependencies))
				{
					result_ = DeltaIterationResults::FailingComplement;
					return;
				}
			}

			Out::Verb() << "Iteration " << iteration_ << ": smaller subset not found.\n";
			result_ = DeltaIterationResults::Passing;
		}
	};
}

#endif
