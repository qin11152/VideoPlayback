#include "module/VideoDecoder/VideoDecoder.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

class VideoDecoderTest : public ::testing::Test
{
public:
	VideoDecoder videoDecoder;

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

TEST_F(VideoDecoderTest, DISABLED_DifferentFileNameInputTest)
{
	VideoInfo outVideoInfo;
	outVideoInfo.width = 1920;
	outVideoInfo.height = 1080;
	outVideoInfo.fps = 25;
	outVideoInfo.videoFormat = AV_PIX_FMT_YUV422P;
	AudioInfo outAudioInfo;
	outAudioInfo.audioChannels = 2;
	outAudioInfo.audioSampleRate = 48000;
	outAudioInfo.audioFormat = AV_SAMPLE_FMT_S16;

	EXPECT_EQ(videoDecoder.initModule("D:/1.mov", outVideoInfo, outAudioInfo), (int32_t)ErrorCode::OpenInputError);
	EXPECT_EQ(videoDecoder.initModule("D:/2.mxf", outVideoInfo, outAudioInfo), (int32_t)ErrorCode::NoError);
}

TEST_F(VideoDecoderTest, MedioInfoVerifity)
{
	VideoInfo outVideoInfo;
	outVideoInfo.width = 1920;
	outVideoInfo.height = 1080;
	outVideoInfo.videoFormat = AV_PIX_FMT_YUV422P;
	AudioInfo outAudioInfo;
	outAudioInfo.audioChannels = 2;
	outAudioInfo.audioSampleRate = 48000;
	outAudioInfo.bitDepth = 8;
	outAudioInfo.audioFormat = AV_SAMPLE_FMT_S16;
	EXPECT_EQ(videoDecoder.initModule("D:/1.mp4", outVideoInfo, outAudioInfo), (int32_t)ErrorCode::NoError);

	auto videoCodecContext = videoDecoder.getVideoCodecContext();
	auto audioCodexContext = videoDecoder.getAudioCodecContext();

	EXPECT_EQ(videoCodecContext->width, 1920);
	EXPECT_EQ(videoCodecContext->height, 1080);
	EXPECT_EQ(videoCodecContext->framerate.num, 50);
	EXPECT_EQ(audioCodexContext->ch_layout.nb_channels, 2);
	EXPECT_EQ(audioCodexContext->sample_rate, 48000);
	//EXPECT_EQ(audioCodexContext->sample_fmt, AV_SAMPLE_FMT_S16);
}



