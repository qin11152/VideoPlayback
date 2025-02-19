#include "module/decoderedDataHandler/PreviewAndPlay/AtomPreviewAndPlay.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

class AtomPreviewAndPlayTest : public testing::Test
{
public:
	AtomPreviewAndPlayTest()
	{
	}
	~AtomPreviewAndPlayTest()
	{
	}
	virtual void SetUp()
	{
		m_ptrAtomPreviewAndPlay = new AtomPreviewAndPlay();
	}
	virtual void TearDown()
	{
		delete m_ptrAtomPreviewAndPlay;
	}
	AtomPreviewAndPlay* m_ptrAtomPreviewAndPlay;
};

TEST_F(AtomPreviewAndPlayTest, AtomPreviewAndPlayInited)
{
	DataHandlerInitedInfo info;
	info.uiNeedSleepTime = 25;
	EXPECT_EQ(m_ptrAtomPreviewAndPlay->initModule(info), 0);
	EXPECT_EQ(m_ptrAtomPreviewAndPlay->uninitModule(), 0);
}

TEST_F(AtomPreviewAndPlayTest, AtomPreviewAndPlaySetVideoQueue)
{
	std::shared_ptr<MyPacketQueue<std::shared_ptr<DecodedImageInfo>>> ptr = std::make_shared<MyPacketQueue<std::shared_ptr<DecodedImageInfo>>>();
	EXPECT_EQ(m_ptrAtomPreviewAndPlay->setVideoQueue(ptr), 0);
}

TEST_F(AtomPreviewAndPlayTest, AtomPreviewAndPlaySetAudioQueue)
{
	std::shared_ptr<std::vector<std::shared_ptr<Buffer>>> ptr = std::make_shared<std::vector<std::shared_ptr<Buffer>>>();
	EXPECT_EQ(m_ptrAtomPreviewAndPlay->setAudioQueue(ptr), 0);
}
