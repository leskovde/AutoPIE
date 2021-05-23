#ifndef CONSUMERS_NAIVE_H
#define CONSUMERS_NAIVE_H
#pragma once

#include <clang/AST/ASTConsumer.h>

#include <utility>

#include "Visitors.h"
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

			//out::All() << "Maximum expected variants: " << pow(2, numberOfCodeUnits) << "\n";

			// The first epoch requires special handling.
			// All valid bitmask variants should be iterated and separated into bins based on their size.
			// The bins are then iterated in their respective epochs.
			if (globalContext_.currentEpoch == 0)
			{
				out::All() << "Binning variants...\n";

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

				auto bitMask = BitMask(numberOfCodeUnits);

				// Distribute bit masks into ranges.
				while (!IsFull(bitMask))
				{
					Increment(bitMask);

					const auto validation = IsValid(bitMask, dependencies);

					if (validation.first)
					{
						auto it = globalContext_.deepeningContext.bitMasks.upper_bound(validation.second);
						it->second.push_back(bitMask);
					}
				}
			}

			auto variantsCount = 0;
			auto& bitMasks = globalContext_.deepeningContext.bitMasks.lower_bound((globalContext_.currentEpoch + 1) * globalContext_.deepeningContext.epochStep - globalContext_.deepeningContext.epochStep / 2)->second;

			for (auto& bitMask : bitMasks)
			{
				variantsCount++;

				if (variantsCount % 50 == 0)
				{
					out::All() << "Done " << variantsCount << " variants.\n";
				}

				out::Verb() << "DEBUG: Processing valid bitmask " << Stringify(bitMask) << "\n";

				try
				{
					auto fileName = TempFolder + std::to_string(variantsCount) + "_" + GetFileName(globalContext_.parsedInput.errorLocation.filePath) + LanguageToExtension(globalContext_.language);
					printingConsumer_.HandleTranslationUnit(context, fileName, bitMask);

					globalContext_.variantAdjustedErrorLocations[variantsCount] = printingConsumer_.GetAdjustedErrorLines();
				}
				catch (...)
				{
					out::All() << "Could not process iteration no. " << variantsCount << " due to an internal exception.\n";
				}
			}

			out::All() << "Finished. Done " << variantsCount << " variants.\n";
		}
	};
}

#endif