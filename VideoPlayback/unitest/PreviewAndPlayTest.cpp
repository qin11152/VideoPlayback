#include "module/decoderedDataHandler/PreviewAndPlay/PreviewAndPlay.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

class PreviewAndPlayTest : public testing::Test
{
public:
	PreviewAndPlayTest() = default;
	~PreviewAndPlayTest() = default;
	void SetUp() override
	{
		m_ptrPreviewAndPlay = new PreviewAndPlay();
		// Code here will be called immediately after the constructor (right before each test).
	}
	void TearDown() override
	{
		delete m_ptrPreviewAndPlay;
		// Code here will be called immediately after each test (right before the destructor).
	}
	PreviewAndPlay* m_ptrPreviewAndPlay;
};

TEST_F(PreviewAndPlayTest, previewAndPlayInit)
{
	DataHandlerInitedInfo info;
	info.uiNeedSleepTime = 25;
	EXPECT_EQ(m_ptrPreviewAndPlay->initModule(info), 0);
	EXPECT_EQ(m_ptrPreviewAndPlay->uninitModule(), 0);
}

TEST_F(PreviewAndPlayTest, previewAndPlaySetVideoQueue)
{
	std::shared_ptr<MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>> ptr = std::make_shared<MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>>();
	EXPECT_EQ(m_ptrPreviewAndPlay->setVideoQueue(ptr), 0);
}

TEST_F(PreviewAndPlayTest, previewAndPlaySetAudioQueue)
{
	std::shared_ptr<Buffer> ptr = std::make_shared<Buffer>();
	EXPECT_EQ(m_ptrPreviewAndPlay->setAudioQueue(ptr), 0);
}
