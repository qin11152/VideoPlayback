#pragma once

#include "CommonDef.h"

namespace utils
{
	AVHWDeviceType getSupportedHWDeviceType();

	std::string getTime(const std::chrono::system_clock::time_point currentTime);

	void preciseSleep(std::chrono::microseconds duration);

}
