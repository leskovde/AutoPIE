#include <clang/Tooling/Tooling.h>

#include "../include/Actions.h"

/**
 * Creates a `VariantGenerationFrontendActionFactory` with a given `GlobalContext` member.\n
 * Any `VariantGenerationFrontendAction`s created by the factory then have the `GlobalContext` member as well.\n
 * This extra step is required due to the default `FrontendActionFactory` not having support for custom constructors
 * and passing data to created instances.
 *
 * @param context A reference to the global context which should be passed onto created instances.
 * @return A `VariantGenerationFrontendActionFactory` instance with the given context as a member.
 */
std::unique_ptr<clang::tooling::FrontendActionFactory> CustomFrontendActionFactory(GlobalContext& context)
{
	class VariantGenerationFrontendActionFactory : public clang::tooling::FrontendActionFactory
	{
		GlobalContext& context_;

	public:

		explicit VariantGenerationFrontendActionFactory(GlobalContext& context) : context_(context)
		{
		}

		std::unique_ptr<clang::FrontendAction> create() override
		{
			return std::make_unique<VariantGenerationAction>(context_);
		}
	};

	return std::unique_ptr<clang::tooling::FrontendActionFactory>(
		std::make_unique<VariantGenerationFrontendActionFactory>(context));
}
