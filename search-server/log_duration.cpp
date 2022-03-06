#include "log_duration.h"

#include <chrono>
#include <iostream>
#include <string>

LogDuration::LogDuration(const std::string& id, std::ostream& out)
	: id_(id), out_(out)
{
}

LogDuration::~LogDuration()
{
	using namespace std::chrono;
	using namespace std::literals;

	const auto end_time = Clock::now();
	const auto dur = end_time - start_time_;
	out_ << id_ << ": "s << duration_cast<milliseconds>(dur).count() << " ms"s << "\n";
}