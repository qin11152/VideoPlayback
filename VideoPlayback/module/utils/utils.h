#pragma once

#include "CommonDef.h"

namespace utils
{
	MY_EXPORT AVHWDeviceType getSupportedHWDeviceType();

	MY_EXPORT std::string getTime(const std::chrono::system_clock::time_point currentTime);

	MY_EXPORT void preciseSleep(std::chrono::microseconds duration);

	MY_EXPORT void preciseSleep(double milliseconds, bool bPrintTrueSleepTime = false);

	int ConvertToUYVY422(const AVFrame* src_frame, uint8_t* dst_buffer, int dst_stride);

#if defined(WIN32)
	MY_EXPORT std::wstring BSTRToWString(const BSTR bstr);
	
	MY_EXPORT std::string WStringToString(const std::wstring& wstr);
#endif
}
