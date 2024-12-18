#pragma once

#include "CommonDef.h"

namespace utils
{
	MY_EXPORT AVHWDeviceType getSupportedHWDeviceType();

	MY_EXPORT std::string getTime(const std::chrono::system_clock::time_point currentTime);

	MY_EXPORT void preciseSleep(std::chrono::microseconds duration);

}
