#pragma once

#include "CommonDef.h"

namespace utils
{
	MY_EXPORT AVHWDeviceType getSupportedHWDeviceType();

	MY_EXPORT std::string getTime(const std::chrono::system_clock::time_point currentTime);

	MY_EXPORT void preciseSleep(std::chrono::microseconds duration);

	MY_EXPORT void preciseSleep(double milliseconds);

#if defined(WIN32)
	MY_EXPORT std::wstring BSTRToWString(const BSTR bstr);
	
	MY_EXPORT std::string WStringToString(const std::wstring& wstr);
#endif
}
