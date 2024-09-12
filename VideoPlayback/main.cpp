#include "VideoPlayback.h"
#include <QtWidgets/QApplication>

#include <gmock/gmock.h>

#define UNITTEST 0

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
#if UNITTEST
    testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
#endif
    LogConfig conf = {
        .level = "info",
        .path = "log/VideoPlayback.log",
        .size = 5 * 1024 * 1024,
        .count = 10,
    };
    INITLOG(conf);
    LOG_INFO("VideoPlayback start");
    VideoPlayback w;
    w.show();
    return a.exec();
}
