#include "Actions.h"

#include <clang/Tooling/Tooling.h>

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
