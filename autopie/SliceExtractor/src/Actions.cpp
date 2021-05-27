#include <clang/Tooling/Tooling.h>

#include "../include/Actions.h"

namespace SliceExtractor
{
	/**
	 * Creates a `SliceExtractorFrontendActionFactory` with a given container for collecting line numbers.\n
	 * This extra step is required due to the default `FrontendActionFactory` not having support for custom constructors
	 * and passing data to created instances.
	 *
	 * @param lines A container of lines of the slice.
	 * @return A `SliceExtractorFrontendActionFactory` instance with the given context as a member.
	 */
	std::unique_ptr<clang::tooling::FrontendActionFactory> SliceExtractorFrontendActionFactory(std::vector<int>& lines)
	{
		class SliceExtractorFrontendActionFactory : public clang::tooling::FrontendActionFactory
		{
			std::vector<int>& lines_;

		public:

			explicit SliceExtractorFrontendActionFactory(std::vector<int>& lines) : lines_(lines)
			{
			}

			std::unique_ptr<clang::FrontendAction> create() override
			{
				return std::make_unique<SliceExtractorAction>(lines_);
			}
		};

		return std::unique_ptr<clang::tooling::FrontendActionFactory>(
			std::make_unique<SliceExtractorFrontendActionFactory>(lines));
	}
} // namespace SliceExtractor
