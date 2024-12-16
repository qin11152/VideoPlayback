#include "utils.h"

#include <set>

namespace utils
{
	AVHWDeviceType getSupportedHWDeviceType()
	{
		std::set<AVHWDeviceType> setSupportType;
		std::vector<AVHWDeviceType> vecWanted
		{ 
			AV_HWDEVICE_TYPE_CUDA,	//cuda
			AV_HWDEVICE_TYPE_QSV,	//intel
			AV_HWDEVICE_TYPE_VIDEOTOOLBOX,	//mac
			AV_HWDEVICE_TYPE_VAAPI,	//intel or amd 
			AV_HWDEVICE_TYPE_D3D11VA
		};
		enum AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;
		while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE) 
		{
			// 检查此硬件类型支持的配置
			AVHWDeviceContext* device_ctx = nullptr;
			AVBufferRef* device_ref = nullptr;
			int err = av_hwdevice_ctx_create(&device_ref, type, nullptr, nullptr, 0);
			if (err < 0) 
			{
				continue;
			}
			setSupportType.insert(type);
		}
		type = AV_HWDEVICE_TYPE_NONE;
		for (auto item : vecWanted)
		{
			if (setSupportType.count(item))
			{
				type = item;
				break;
			}
		}
		return type;
	}

	std::string getTime(const std::chrono::system_clock::time_point currentTime)
	{
		auto in_time_t = std::chrono::system_clock::to_time_t(currentTime);
		std::tm tm = *std::localtime(&in_time_t);

		auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch()) % 1000;

		std::ostringstream oss;
		oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
		oss << '.' << std::setw(3) << std::setfill('0') << milliseconds.count();
		return oss.str();
	}

	void preciseSleep(std::chrono::microseconds duration)
	{
		auto start = std::chrono::high_resolution_clock::now();
		while (std::chrono::high_resolution_clock::now() - start < duration) {
			// 忙等待
		}
	}

}