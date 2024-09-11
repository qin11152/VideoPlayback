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
    VideoPlayback w;
    w.show();
    return a.exec();
}
