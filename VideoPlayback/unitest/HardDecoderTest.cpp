#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "module/VideoDecoder/HardDecoder.h"
#include "module/demux/demuxer.h"

class HardDecoderTest : public testing::Test
{
public:
	HardDecoderTest() = default;
	~HardDecoderTest() = default;
	void SetUp() override
	{
		std::shared_ptr<demuxer> ptrDemuxer = std::make_shared<demuxer>();
		m_ptrHardDecoder = new HardDecoder(ptrDemuxer);
		// Code here will be called immediately after the constructor (right before each test).
	}
	void TearDown() override
	{
		delete m_ptrHardDecoder;
		// Code here will be called immediately after each test (right before the destructor).
	}
	HardDecoder* m_ptrHardDecoder;
};

TEST_F(HardDecoderTest, harderDecoderInited)
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
	EXPECT_EQ(m_ptrHardDecoder->initModule(decoderInfo, dataHandlerInfo), 0);

	EXPECT_NE(dataHandlerInfo.uiNeedSleepTime, 0);
	EXPECT_NE(dataHandlerInfo.uiPerFrameSampleCnt, 0);

	EXPECT_EQ(tmp.uninitModule(), 0);
	EXPECT_EQ(m_ptrHardDecoder->uninitModule(), 0);
}

TEST_F(HardDecoderTest, harderDecoderAddVideoQueue)
{
	std::shared_ptr < MyPacketQueue<std::shared_ptr<DecodedImageInfo>>> tmp = std::make_shared<MyPacketQueue<std::shared_ptr<DecodedImageInfo>>>();
	EXPECT_EQ(m_ptrHardDecoder->addPacketQueue(tmp), 0);
	EXPECT_EQ(m_ptrHardDecoder->addPacketQueue(tmp), -1);
}

TEST_F(HardDecoderTest, harderDecoderAddPcmBuffer)
{
	std::shared_ptr<Buffer> tmp = std::make_shared<Buffer>();
	EXPECT_EQ(m_ptrHardDecoder->addPCMBuffer(tmp), 0);
	EXPECT_EQ(m_ptrHardDecoder->addPCMBuffer(tmp), -1);
}
