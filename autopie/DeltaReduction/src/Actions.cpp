#include <clang/Tooling/Tooling.h>

#include "../../Common/include/Context.h"
#include "../include/Actions.h"

namespace Delta
{
	/**
	 * Creates a `DeltaDebuggingFrontendActionFactory` with a given `GlobalContext` member.\n
	 * Any `DeltaDebuggingFrontendAction`s created by the factory then have the `GlobalContext` member as well.\n
	 * This extra step is required due to the default `FrontendActionFactory` not having support for custom constructors
	 * and passing data to created instances.
	 *
	 * @param context A reference to the global context which should be passed onto created instances.
	 * @param iteration The number of the current DD iteration (used for file manipulation).
	 * @param partitionCount The number of subsets to be tested.
	 * @param result The exit status of the iteration.
	 * @return A `DeltaDebuggingFrontendActionFactory` instance with the given context as a member.
	 */
	std::unique_ptr<clang::tooling::FrontendActionFactory> DeltaDebuggingFrontendActionFactory(
		GlobalContext& context, const int iteration, const int partitionCount, DeltaIterationResults& result)
	{
		class DeltaDebuggingFrontendActionFactory : public clang::tooling::FrontendActionFactory
		{
			int iteration_;
			int partitionCount_;
			GlobalContext& context_;
			DeltaIterationResults& result_;

		public:

			DeltaDebuggingFrontendActionFactory(GlobalContext& context, const int iteration,
			                                    const int partitionCount,
			                                    DeltaIterationResults& result) : iteration_(iteration),
			                                                                     partitionCount_(partitionCount),
			                                                                     context_(context), result_(result)
			{
			}

			std::unique_ptr<clang::FrontendAction> create() override
			{
				return std::make_unique<DeltaDebuggingAction>(context_, iteration_, partitionCount_, result_);
			}
		};

		return std::unique_ptr<clang::tooling::FrontendActionFactory>(
			std::make_unique<DeltaDebuggingFrontendActionFactory>(context, iteration, partitionCount, result));
	}
} // namespace Delta
