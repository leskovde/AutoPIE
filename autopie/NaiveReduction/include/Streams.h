#ifndef STREAMS_H
#define STREAMS_H
#pragma once

#include <chrono>
#include <iostream>
#include <fstream>

#include "Options.h"

namespace out
{
	typedef std::ostream& (*Manipulator) (std::ostream&);
	
	static std::_Timeobj<char, const tm*> GetTimestamp()
	{
		const auto timeStamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		
		return std::put_time(std::gmtime(&timeStamp), "%Y-%m-%d %H:%M:%S");
	}
	
	struct Logger
	{
		std::ofstream ofs;

		explicit Logger(const char* filePath) : ofs(filePath)
		{
		}
	};
	
	struct FilteredLogger
	{
		Logger& logger;

		explicit FilteredLogger(Logger& logger) : logger(logger)
		{
		}
	};

	inline Logger all_ = Logger(LogFile);
	inline FilteredLogger verb_ = FilteredLogger(all_);

	inline Logger& All()
	{
		if (LogToFile)
		{
			all_.ofs << GetTimestamp() << ":\t";
		}

		return all_;
	}

	inline FilteredLogger& Verb()
	{
		if (Verbose)
		{
			if (LogToFile)
			{
				verb_.logger.ofs << GetTimestamp() << ":\t";
			}
		}
		
		return verb_;
	}
	
	template <class T> Logger& operator<< (Logger& logger, const T& x)
	{
		std::cout << x;

		if (LogToFile)
		{			
			logger.ofs << x;
		}

		return logger;
	}

	inline Logger& operator<< (Logger& logger, const Manipulator manipulator)
	{
		std::cout << manipulator;

		if (LogToFile)
		{
			logger.ofs << manipulator;
		}

		return logger;
	}
	
	template <class T> FilteredLogger& operator<< (FilteredLogger& logger, const T& x)
	{
		if (Verbose)
		{
			std::cout << x;

			if (LogToFile)
			{
				logger.logger.ofs << x;
			}
		}
		
		return logger;
	}

	inline FilteredLogger& operator<< (FilteredLogger& logger, const Manipulator manipulator)
	{
		if (Verbose)
		{
			std::cout << manipulator;

			if (LogToFile)
			{
				logger.logger.ofs << manipulator;
			}
		}
		
		return logger;
	}
}

#endif
