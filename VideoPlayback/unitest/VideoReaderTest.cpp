#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "module/VideoReader/VideoReader.h"

class VideoReaderTest : public testing::Test
{
public:
	VideoReaderTest() = default;
	~VideoReaderTest() = default;
	void SetUp() override
	{
		m_ptrVideoReader = new VideoReader();
		// Code here will be called immediately after the constructor (right before each test).
	}
	void TearDown() override
	{
		delete m_ptrVideoReader;
		// Code here will be called immediately after each test (right before the destructor).
	}
	VideoReader* m_ptrVideoReader;
};

TEST_F(VideoReaderTest, videoDecoderInit)
{
	VideoInfo videoInfo;
	AudioInfo audioInfo;
	VideoReaderInitedInfo info;
	DecoderInitedInfo decoderInfo;

	std::shared_ptr<MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>> ptrPacket = std::make_shared<MyPacketQueue<std::shared_ptr<PacketWaitDecoded>>>();

	info.outAudioInfo = audioInfo;
	info.outVideoInfo = videoInfo;
	info.ptrPacketQueue = nullptr;
	info.m_strFileName = "D:/testmaterial/1.mp4";
	info.ptrPacketQueue = ptrPacket;
	
	EXPECT_EQ(m_ptrVideoReader->initModule(info, decoderInfo), 0);
	EXPECT_NE(decoderInfo.audioCodec, nullptr);
	EXPECT_NE(decoderInfo.audioCodecParameters, nullptr);
	EXPECT_NE(decoderInfo.videoCodec, nullptr);
	EXPECT_NE(decoderInfo.videoCodecParameters, nullptr);
	EXPECT_NE(decoderInfo.formatContext, nullptr);
	EXPECT_NE(decoderInfo.iAudioIndex, -1);
	EXPECT_NE(decoderInfo.iVideoIndex, -1);
	EXPECT_NE(decoderInfo.ptrPacketQueue, nullptr);
	EXPECT_EQ(m_ptrVideoReader->uninitModule(), 0);
}

TEST_F(VideoReaderTest, videoDecoderUninited)
{
	EXPECT_EQ(m_ptrVideoReader->uninitModule(), -1);
}
