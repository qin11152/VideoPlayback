#include "module/VideoDecoder/VideoDecoder.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class VideoDecoderTest : public ::testing::Test
{
public:
	std::shared_ptr<VideoDecoder> videoDecoder;

protected:
	void SetUp() override
	{
		std::shared_ptr<VideoReader> ptrVideoReader = std::make_shared<VideoReader>();
		videoDecoder = std::make_shared<VideoDecoder>(ptrVideoReader);
		// Initialization code here
	}

	void TearDown() override
	{
		// Code here will be called immediately after each test
	}
};

TEST_F(VideoDecoderTest, videoDecoderInit)
{
	VideoReader tmp;
	VideoInfo videoInfo;
	videoInfo.width = 1920;
	videoInfo.height = 1080;
	videoInfo.fps = 25;
	videoInfo.videoFormat = AV_PIX_FMT_YUV422P;
	AudioInfo audioInfo;
	audioInfo.audioChannels = 2;
	audioInfo.audioSampleRate = 48000;
	audioInfo.audioFormat = AV_SAMPLE_FMT_S16;

	VideoReaderInitedInfo info;
	DecoderInitedInfo decoderInfo;
	DataHandlerInitedInfo dataHandlerInfo;

	info.outAudioInfo = audioInfo;
	info.outVideoInfo = videoInfo;
	info.ptrPacketQueue = nullptr;
	info.m_strFileName = "D:/testmaterial/1.mp4";
	info.m_eDeviceType = AV_HWDEVICE_TYPE_QSV;

	ASSERT_EQ(tmp.initModule(info, decoderInfo), 0);
	EXPECT_EQ(videoDecoder->initModule(decoderInfo, dataHandlerInfo), 0);

	EXPECT_NE(dataHandlerInfo.uiNeedSleepTime, 0);
	EXPECT_NE(dataHandlerInfo.uiPerFrameSampleCnt, 0);

	EXPECT_EQ(tmp.uninitModule(), 0);
	EXPECT_EQ(videoDecoder->uninitModule(), 0);
}




