#ifndef STREAMS_H
#define STREAMS_H
#pragma once

#include <chrono>
#include <iostream>
#include <fstream>

#include "Options.h"

/**
 * Contains members for outputting messages in a custom manner.\n
 * The goal is to provide multiple logging levels with different levels of details.
 */
namespace Out
{
	typedef std::ostream& (*Manipulator)(std::ostream&);

	/**
	 * Retrieves the current time (Greenwich Mean Time).
	 *
	 * @return Current GMT represented by the `tm` structure.
	 */
	static tm* GetTimestamp()
	{
		const auto timeStamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

		return std::gmtime(&timeStamp);
	}

	/**
	 * Represent an ordinary output stream - one that outputs messages without any constraints.\n
	 * It is expected that the user does not create additional instances. Instead, instantiated members
	 * of this namespace should be used.
	 */
	struct Logger
	{
		bool initialized = false;
		std::ofstream ofs;

		explicit Logger(const char* filePath)
		{
			try
			{
				if (LogToFile)
				{
					ofs.open(filePath);
					initialized = true;
				}
			}
			catch (std::ios_base::failure& e)
			{
				std::cerr << "The log file could not be initialized."
					<< "If you are using the `--log` option, no output will be logged.\n"
					<< "Error: " << e.what() << "\n";

				initialized = false;
			}
		}
	};

	/**
	 * Represent a constrained output stream.\n
	 * It is expected that the user does not create additional instances. Instead, instantiated members
	 * of this namespace should be used.
	 */
	struct FilteredLogger
	{
		Logger& logger;

		explicit FilteredLogger(Logger& logger) : logger(logger)
		{
		}
	};

	inline Logger all_ = Logger(LogFile);
	inline FilteredLogger verb_ = FilteredLogger(all_);

	/**
	 * A stream that outputs messages independently on the `Verbose` option.\n
	 * If the `Log` option is specified, the output is written both to the standard output and to
	 * the default .log file. Log file entries are prefixed with a time stamp for each line.
	 *
	 * Usage: `Out::All() << "foo" << "bar";`
	 */
	inline Logger& All()
	{
		if (all_.initialized)
		{
			all_.ofs << std::put_time(GetTimestamp(), "%Y-%m-%d %H:%M:%S") << ":\t";
		}

		return all_;
	}

	/**
	 * A stream that outputs messages only when the `Verbose` option is specified at launch.\n
	 * Additionally, if both the `Verbose` and `Log` options are specified, the output is written
	 * both to the standard output and to the default .log file. Log file entries are prefixed
	 * with a time stamp for each line.
	 *
	 * Usage: `Out::Verb() << "foo" << "bar";`
	 */
	inline FilteredLogger& Verb()
	{
		if (Verbose)
		{
			if (verb_.logger.initialized)
			{
				verb_.logger.ofs << std::put_time(GetTimestamp(), "%Y-%m-%d %H:%M:%S") << ":\t";
			}
		}

		return verb_;
	}

	/**
	 * Handles general types sent to the non-constrained stream.\n
	 * If the `Log` option is specified, the output is written both to the standard output and to
	 * the default .log file.
	 */
	template <class T>
	Logger& operator<<(Logger& logger, const T& x)
	{
		std::cout << x;

		if (logger.initialized)
		{
			logger.ofs << x;
		}

		return logger;
	}

	/**
	 * Handles the LLVM string reference sent to the non-constrained stream.\n
	 * If the `Log` option is specified, the output is written both to the standard output and to
	 * the default .log file.
	 */
	inline Logger& operator<<(Logger& logger, const llvm::StringRef stringRef)
	{
		if (Verbose)
		{
			std::cout << stringRef.str();

			if (logger.initialized)
			{
				logger.ofs << stringRef.str();
			}
		}

		return logger;
	}

	/**
	 * Handles types already processed by a previous `<<` operator sent to the non-constrained stream.\n
	 * If the `Log` option is specified, the output is written both to the standard output and to
	 * the default .log file.
	 */
	inline Logger& operator<<(Logger& logger, const Manipulator manipulator)
	{
		std::cout << manipulator;

		if (logger.initialized)
		{
			logger.ofs << manipulator;
		}

		return logger;
	}

	/**
	 * Handles general types sent to the filtered stream.\n
	 * If both the `Verbose` and `Log` options are specified, the output is written
	 * both to the standard output and to the default .log file.
	 */
	template <class T>
	FilteredLogger& operator<<(FilteredLogger& logger, const T& x)
	{
		if (Verbose)
		{
			std::cout << x;

			if (logger.logger.initialized)
			{
				logger.logger.ofs << x;
			}
		}

		return logger;
	}

	/**
	 * Handles the LLVM string reference sent to the filtered stream.\n
	 * If both the `Verbose` and `Log` options are specified, the output is written
	 * both to the standard output and to the default .log file.
	 */
	inline FilteredLogger& operator<<(FilteredLogger& logger, const llvm::StringRef stringRef)
	{
		if (Verbose)
		{
			std::cout << stringRef.str();

			if (logger.logger.initialized)
			{
				logger.logger.ofs << stringRef.str();
			}
		}

		return logger;
	}

	/**
	 * Handles types already processed by a previous `<<` operator sent to the filtered stream.\n
	 * If both the `Verbose` and `Log` options are specified, the output is written
	 * both to the standard output and to the default .log file.
	 */
	inline FilteredLogger& operator<<(FilteredLogger& logger, const Manipulator manipulator)
	{
		if (Verbose)
		{
			std::cout << manipulator;

			if (logger.logger.initialized)
			{
				logger.logger.ofs << manipulator;
			}
		}

		return logger;
	}
}

#endif
