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
		GlobalContext& globalContext_;

	public:
		DeltaDebuggingConsumer(clang::CompilerInstance* ci, GlobalContext& context) : mappingConsumer_(ci, context),
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
			auto partitionCount = 2;
			
			auto iteration = 0;
			const auto cutOffLimit = 0xffff;

			while (iteration < cutOffLimit)
			{
				iteration++;

				if (iteration % 50 == 0)
				{
					out::All() << "Done " << iteration << " DD iterations.\n";
				}

				// TODO: Feed the current file to the mapping consumer.
				
				mappingConsumer_.HandleTranslationUnit(context);
				const auto numberOfCodeUnits = mappingConsumer_.GetCodeUnitsCount();

				globalContext_.variantAdjustedErrorLocation.clear();
				printingConsumer_.SetData(mappingConsumer_.GetSkippedNodes(), mappingConsumer_.GetDependencyGraph());

				auto dependencies = mappingConsumer_.GetDependencyGraph();
				
				// 1. Split into a container of equal sized bins.
				// 2. Get a container of complements.
				// 3. Loop over the first container and test each bin (generate variant and execute).
				// 4. If a variant fails, set n to 2 and the file to that variant.
				// 5. Loop over the second container and test --||--.
				// 6. If a variant fails, decrement n and set the file to that variant/
				// 7. If nothing fails, set n to 2 *n.

				std::vector<BitMask> partitions;
				std::vector<BitMask> complements;
				const auto partitionSize = numberOfCodeUnits / partitionCount;
				
				for (auto i = 0; i < partitionCount; i++)
				{
					auto partition = BitMask(numberOfCodeUnits);
					auto complement = BitMask(numberOfCodeUnits);

					// Assign code units into partitions.
					for (auto j = 0; j < numberOfCodeUnits; j++)
					{
						if (i * partitionSize <= j && (j < (i + 1) * partitionSize || i == partitionCount - 1))
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

					partitions.emplace_back(partition);
					complements.emplace_back(complement);
				}

				for (auto& partition : partitions)
				{
					
				}

				for (auto& complement : complements)
				{
					
				}

				// No smaller subset found, increase granularity.
				partitionCount *= 2;

				if (partitionCount > numberOfCodeUnits)
				{
					
				}
				
				try
				{
					auto fileName = TempFolder + std::to_string(iteration) + "_" + GetFileName(globalContext_.parsedInput.errorLocation.filePath) + LanguageToExtension(globalContext_.language);
					printingConsumer_.HandleTranslationUnit(context, fileName);

					globalContext_.variantAdjustedErrorLocation[iteration] = printingConsumer_.GetAdjustedErrorLine();
				}
				catch (...)
				{
					out::All() << "Could not process iteration no. " << iteration << " due to an internal exception.\n";
				}
			}

			out::All() << "Finished. Done " << iteration << " DD iterations.\n";
		}
	};
}

#endif