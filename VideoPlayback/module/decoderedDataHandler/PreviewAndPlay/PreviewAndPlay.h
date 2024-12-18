#pragma once

#include "CommonDef.h"
#include "module/MyContainer/MyQueue.h"
#include "module/MyContainer/Buffer.h"
#include "module/decoderedDataHandler/PcmDatahandler.h"
#include "module/decoderedDataHandler/YuvDataHandler.h"

class MY_EXPORT PreviewAndPlay : public PcmDatahandler, public YuvDataHandler, public std::enable_shared_from_this<PreviewAndPlay>
{
public:
	PreviewAndPlay();
	~PreviewAndPlay()override;

	int32_t initModule(const DataHandlerInitedInfo& info) override;
	int32_t uninitModule() override;

	void setCallback(YuvCallBack callback) override;
	void setCallback(AudioPlayCallback callback) override;
	void setFinishedCallback(YuvFinishedCallback callback) override;

	void pause() override;
	void resume() override;

	int32_t setVideoQueue(std::shared_ptr <MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>> ptrQueueDecodedVideo);
	int32_t setAudioQueue(std::shared_ptr<Buffer> ptrPcmBuffer);

	void setDecoderFinshedState(bool state);

	std::shared_ptr<YuvDataHandler> getYuvDataHandler();
	std::shared_ptr<PcmDatahandler> getPcmDataHandler();

private:
	void handler();

private:
	bool m_bInitState{ false };
	bool m_bRunningState{ false };

	std::atomic<bool> m_bDecoderFinished{ false };

	bool m_bPauseState{ false };
	std::mutex m_PauseMutex;
	std::condition_variable m_PauseCV; // 暂停时的条件变量

	std::thread m_ConsumeThread;

	std::mutex m_VideoQueueMutex;
	std::shared_ptr <MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>> m_ptrQueueDecodedVideo;

	std::mutex m_PcmBufferMutex;
	std::shared_ptr<Buffer> m_ptrPcmBuffer;

	uint32_t m_uiConsumeThreadSleepTime{ 0 };
	uint32_t m_uiPerFrameSampleCnt{ 0 };

};

