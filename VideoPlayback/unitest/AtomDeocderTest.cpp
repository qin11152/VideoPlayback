#include "module/VideoDecoder/AtomDecoder.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>


class AtomDecoderTest : public testing::Test
{
public:
	AtomDecoderTest()
	{
	}
	~AtomDecoderTest()
	{
	}
	virtual void SetUp()
	{
		m_ptrAtomDecoder = new AtomDecoder({});
	}
	virtual void TearDown()
	{
		delete m_ptrAtomDecoder;
	}
	AtomDecoder* m_ptrAtomDecoder;
};

TEST_F(AtomDecoderTest, AtomDecoderInited)
{
	VideoInfo videoInfo;
	videoInfo.width = 1920;
	videoInfo.height = 1080;
	videoInfo.fps = 25;
	videoInfo.videoFormat = AV_PIX_FMT_YUV422P;
	AudioInfo audioInfo;
	audioInfo.audioChannels = 2;
	audioInfo.audioSampleRate = 48000;
	audioInfo.audioFormat = AV_SAMPLE_FMT_S16;

	std::vector<VideoReaderInitedInfo> vecInfo;
	DecoderInitedInfo decoderInfo;
	DataHandlerInitedInfo dataHandlerInfo;

	std::vector<std::shared_ptr<VideoReader>> vecVideoReader;

	std::vector<std::string> vecFile = { "D:/testmaterial/aa.mxf" ,"D:/testmaterial/01.mxf" ,"D:/testmaterial/02.mxf" };
	for (int i = 0; i < vecFile.size(); ++i)
	{
		VideoReaderInitedInfo info;
		info.m_bAtom = true;
		info.m_eDeviceType = AV_HWDEVICE_TYPE_QSV;
		info.m_strFileName = vecFile[i];
		info.outAudioInfo = audioInfo;
		info.outVideoInfo = videoInfo;
		auto tmp = std::make_shared<VideoReader>();
		tmp->initModule(info, decoderInfo);
		vecVideoReader.push_back(tmp);
		vecInfo.push_back(info);
	}

	decoderInfo.outAudioInfo = audioInfo;
	decoderInfo.outVideoInfo = videoInfo;
	decoderInfo.ptrAtomVideoPacketQueue = nullptr;
	decoderInfo.m_eDeviceType = AV_HWDEVICE_TYPE_QSV;

	EXPECT_EQ(m_ptrAtomDecoder->initModule(decoderInfo, dataHandlerInfo), 0);
	EXPECT_NE(dataHandlerInfo.uiNeedSleepTime, 0);
	EXPECT_NE(dataHandlerInfo.uiPerFrameSampleCnt, 0);
	EXPECT_EQ(m_ptrAtomDecoder->uninitModule(), 0);
}
