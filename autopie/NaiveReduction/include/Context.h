#ifndef CONTEXT_H
#define CONTEXT_H
#pragma once

#include "Helper.h"

/**
 * Serves as a container for all publicly available global information.
 * Currently includes the parsed input.
 */
class GlobalContext
{
	GlobalContext(): parsedInput(InputData("", Location("", 0), 0.0, false))
	{
		llvm::errs() << "DEBUG: GlobalContext - New default constructor call.\n";
	}

public:

	// Variant generation properties.
	InputData parsedInput;

	GlobalContext(InputData& input, const std::string& /*source*/) : parsedInput(input)
	{
		llvm::errs() << "DEBUG: GlobalContext - New non-default constructor call.\n";
	}
};
#endif
