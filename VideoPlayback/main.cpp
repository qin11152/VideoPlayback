#include "VideoPlayback.h"
#include <QtWidgets/QApplication>

#include <gmock/gmock.h>
#include "CommonDef.h"
#include "module/utils/utils.h"

#define UNITTEST 0

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
	LogConfig conf("info", "log/VideoPlayback.log", 500 * 1024 * 1024, 10);
	INITLOG(conf);
#if UNITTEST
    testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
#endif
    qRegisterMetaType<VideoCallbackInfo>("VideoCallbackInfo");
    qRegisterMetaType<VideoInfo>("VideoInfo");
    ThreadPool::get_mutable_instance().startPool(10);
    LOG_INFO("VideoPlayback start");
    
    VideoPlayback w;
    w.show();
    return a.exec();
}
