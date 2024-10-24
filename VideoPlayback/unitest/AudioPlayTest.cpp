#include "module//AudioPlay/AudioPlay.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fstream>
#include <thread>

class AudioPlayTest : public testing::Test
{
public:
	AudioPlay* m_ptrAudioPlay{ nullptr };

protected:
	void SetUp() override
	{
		m_ptrAudioPlay = new AudioPlay(nullptr);
	}

	void TearDown() override
	{
		if (m_ptrAudioPlay)
		{
			delete m_ptrAudioPlay;
			m_ptrAudioPlay = nullptr;
		}
	}
};

TEST_F(AudioPlayTest, InitAudioParameter)
{
	AudioInfo audioInfo;
	audioInfo.audioSampleRate = 48000;
	audioInfo.audioChannels = 2;
	audioInfo.audioFormat = AV_SAMPLE_FMT_S16;

	EXPECT_EQ(m_ptrAudioPlay->initOutputParameter(audioInfo), true);
}

TEST_F(AudioPlayTest, DISABLED_inputDataTest)
{
	AudioInfo audioInfo;
	audioInfo.audioSampleRate = 48000;
	audioInfo.audioChannels = 2;
	audioInfo.audioFormat = AV_SAMPLE_FMT_S16;
	
	EXPECT_EQ(m_ptrAudioPlay->initOutputParameter(audioInfo), true);
	m_ptrAudioPlay->startPlay();
	//std::fstream file("D:/audio.pcm", std::ios::in | std::ios::binary);
	//if (file.is_open())
	//{
	//	while (!file.eof())
	//	{
	//		char buffer[4096] = { 0 };
	//		file.read(buffer, 4096);
	//		QByteArray data(buffer, 4096);
	//		m_ptrAudioPlay->inputPcmData(data);
	//		std::this_thread::sleep_for(std::chrono::milliseconds(24));
	//	}
	//}
}
