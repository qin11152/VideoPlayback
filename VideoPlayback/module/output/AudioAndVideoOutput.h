#pragma once

#include "CommonDef.h"

class AudioAndVideoOutput
{
	using YuvCallBack = std::function<void(std::shared_ptr<DecodedImageInfo> videoInfo)>;
	using AudioPlayCallback = std::function< void(std::shared_ptr<DecodedAudioInfo> audioInfo)>;

	using YuvFinishedCallback = std::function<void()>;

public:
	AudioAndVideoOutput();
	~AudioAndVideoOutput();

	int32_t initModule(const DataHandlerInitedInfo& info);
	int32_t uninitModule();

	void setCallback(YuvCallBack callback) { m_YuvCallback = callback; }
	void setCallback(AudioPlayCallback callback) { m_AudioCallback = callback; }
	void setFinishedCallback(YuvFinishedCallback callback) { m_FinishedCallback = callback; }

	void pause();
	void resume();
	void seekTo(SeekParams params);

	void nextFrame();

	void renderPreviousFrame(const SeekParams& params);

	int32_t setVideoQueue(std::shared_ptr <MyPacketQueue<std::shared_ptr<DecodedImageInfo>>> ptrQueueDecodedVideo) {
		m_ptrQueueDecodedVideo = ptrQueueDecodedVideo; return 0;}

	int32_t setAudioQueue(std::shared_ptr<MyPacketQueue<std::shared_ptr<DecodedAudioInfo>>> ptrQueueDecodedAudio) { m_ptrQueueDecodedAudio = ptrQueueDecodedAudio; return 0; }

private:
	void audio();
	void video();

private:
	YuvCallBack m_YuvCallback;
	AudioPlayCallback m_AudioCallback;
	YuvFinishedCallback m_FinishedCallback;

	bool m_bInitState{ false };
	bool m_bRunningState{ false };
	bool m_bVideoReady{ false };

	std::atomic<bool> m_bDecoderFinished{ false };

	bool m_bPauseState{ false };
	std::mutex m_PauseMutex;
	std::condition_variable m_PauseCV; // 暂停时的条件变量

	std::thread m_VideoThread;
	std::thread m_AudioThread;

	std::mutex m_VideoQueueMutex;
	std::shared_ptr <MyPacketQueue<std::shared_ptr<DecodedImageInfo>>> m_ptrQueueDecodedVideo;

	std::mutex m_AudioQueueMutex;
	std::shared_ptr<MyPacketQueue<std::shared_ptr<DecodedAudioInfo>>> m_ptrQueueDecodedAudio;

	uint32_t m_uiConsumeThreadSleepTime{ 0 };
	uint32_t m_uiPerFrameSampleCnt{ 0 };

	bool m_bVideoSeekState{ false };
	bool m_bAudioSeekState{ false };
	double m_dSeekTime{ 0.0 };
	int direction{ 0 };

	std::atomic<double> m_dCurrentAduioPts{ 0.0 };
};

