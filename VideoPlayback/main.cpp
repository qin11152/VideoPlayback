#include "VideoPlayback.h"
#include <QtWidgets/QApplication>

#include <gmock/gmock.h>
#include "CommonDef.h"
#include "module/VideoInfo/VideoInfoAcqure.h"

#define UNITTEST 0

void check_hw_support() {
	enum AVHWDeviceType type = AV_HWDEVICE_TYPE_NONE;

	while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE) {
		std::cout << "Hardware device type: "
			<< av_hwdevice_get_type_name(type) << std::endl;

		// 检查此硬件类型支持的配置
		AVHWDeviceContext* device_ctx = nullptr;
		AVBufferRef* device_ref = nullptr;

		int err = av_hwdevice_ctx_create(&device_ref, type, nullptr, nullptr, 0);
		if (err < 0) {
			std::cout << "Failed to create HW device: " << type << std::endl;
			continue;
		}

		device_ctx = (AVHWDeviceContext*)device_ref->data;
		// 这里可以获取更多设备信息

		av_buffer_unref(&device_ref);
	}
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
#if UNITTEST
    testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
#endif
	check_hw_support();
    qRegisterMetaType<VideoCallbackInfo>("VideoCallbackInfo");
    qRegisterMetaType<VideoInfo>("VideoInfo");
    ThreadPool::get_mutable_instance().startPool(10);
    LogConfig conf("info", "log/VideoPlayback.log", 50 * 1024 * 1024, 10);
    INITLOG(conf);
    LOG_INFO("VideoPlayback start");
    
    VideoPlayback w;
    w.show();
    return a.exec();
}
