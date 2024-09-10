#include "module/VideoInfo/VideoInfoAcqure.h"
#include "ui/VideoPlayback.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class VideoInfoAcqureTest : public ::testing::Test
{
public:
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

TEST_F(VideoInfoAcqureTest,DifferentFileNameInputTest)
{
	MediaInfo mediaInfo;
	EXPECT_EQ(VideoInfoAcqure::getInstance()->getVideoInfo("D:/1.mov", mediaInfo), (int32_t)ErrorCode::OpenInputError);
	//EXPECT_EQ(videoInfoAcqure.getVideoInfo("D:/2.mxf", videoInfo), (int32_t)ErrorCode::NoError);
}

TEST_F(VideoInfoAcqureTest, DISABLED_VideoInfoValid)
{
	MediaInfo mediaInfo;
	EXPECT_EQ(VideoInfoAcqure::getInstance()->getVideoInfo("D:/2.mxf", mediaInfo), (int32_t)ErrorCode::NoError);
	EXPECT_EQ(mediaInfo.width, 1920);
	EXPECT_EQ(mediaInfo.height, 1080);
	EXPECT_EQ(mediaInfo.fps, 25.0);
	EXPECT_EQ(mediaInfo.audioChannels, 8);
	EXPECT_EQ(mediaInfo.audioSampleRate, 48000);
	EXPECT_EQ(mediaInfo.bitDepth, 16);
}

TEST_F(VideoInfoAcqureTest, DISABLED_GetUiChooseFileInfo)
{
	MediaInfo mediaInfo;
	VideoPlayback videoPlayback;
	QString name = videoPlayback.onSignalChooseFileClicked();
	EXPECT_EQ(VideoInfoAcqure::getInstance()->getVideoInfo(name.toStdString().c_str(), mediaInfo), (int32_t)ErrorCode::NoError);
	EXPECT_EQ(mediaInfo.width, 1920);
	EXPECT_EQ(mediaInfo.height, 1080);
	EXPECT_EQ(mediaInfo.fps, 25.0);
	EXPECT_EQ(mediaInfo.audioChannels, 8);
	EXPECT_EQ(mediaInfo.audioSampleRate, 48000);
	EXPECT_EQ(mediaInfo.bitDepth, 16);
}

