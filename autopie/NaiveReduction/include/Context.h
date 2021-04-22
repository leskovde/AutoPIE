#ifndef CONTEXT_H
#define CONTEXT_H
#pragma once

#include "Helper.h"
#include "Streams.h"

/**
 * Serves as a container for all publicly available global information.
 * Currently includes the parsed input.
 */
class GlobalContext
{
	GlobalContext(): parsedInput(InputData("", Location("", 0), 0.0, false))
	{
		out::Verb() << "DEBUG: GlobalContext - New default constructor call.\n";
	}

public:

	// Variant generation properties.
	InputData parsedInput;
	double currentRatioLowerBound{0};
	double currentRationUpperBound{0};
	
	GlobalContext(InputData& input, const std::string& /*source*/) : parsedInput(input)
	{
		out::Verb() << "DEBUG: GlobalContext - New non-default constructor call.\n";
	}
};
#endif
