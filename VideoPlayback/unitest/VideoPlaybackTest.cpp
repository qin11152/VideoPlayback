#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ui/VideoPlayback.h"

class VideoPlaybackTest : public ::testing::Test
{
public:
	VideoPlayback videoPlayback;
protected:
	void SetUp() override
	{
		// Initialization code here
	}

	void TearDown() override
	{
		// Code here will be called immediately after each test
	}
};

TEST_F(VideoPlaybackTest, DISABLED_initModule)
{
	EXPECT_EQ(videoPlayback.initModule(), true);
}

TEST_F(VideoPlaybackTest,DISABLED_getChooseFileName)
{
	QString name = videoPlayback.onSignalChooseFileClicked();
	//EXPECT_EQ(name.isEmpty(), false);
	//EXPECT_THAT(name, testing::Eq(u8"D:/shoulu/融合 高清 路径3 1080i50_20240903_114451_0.mov"));
}
