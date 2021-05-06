#include <clang/Tooling/Tooling.h>

#include "../include/Actions.h"
#include "../../Common/include/Context.h"

namespace Naive
{
	/**
	 * Creates a `VariantGeneratingFrontendActionFactory` with a given `GlobalContext` member.\n
	 * Any `VariantGeneratingFrontendAction`s created by the factory then have the `GlobalContext` member as well.\n
	 * This extra step is required due to the default `FrontendActionFactory` not having support for custom constructors
	 * and passing data to created instances.
	 *
	 * @param context A reference to the global context which should be passed onto created instances.
	 * @return A `VariantGeneratingFrontendActionFactory` instance with the given context as a member.
	 */
	std::unique_ptr<clang::tooling::FrontendActionFactory> VariantGeneratingFrontendActionFactory(GlobalContext& context)
	{
		class VariantGeneratingFrontendActionFactory : public clang::tooling::FrontendActionFactory
		{
			GlobalContext& context_;

		public:

			explicit VariantGeneratingFrontendActionFactory(GlobalContext& context) : context_(context)
			{
			}

			std::unique_ptr<clang::FrontendAction> create() override
			{
				return std::make_unique<VariantGeneratingAction>(context_);
			}
		};

		return std::unique_ptr<clang::tooling::FrontendActionFactory>(
			std::make_unique<VariantGeneratingFrontendActionFactory>(context));
	}
}
