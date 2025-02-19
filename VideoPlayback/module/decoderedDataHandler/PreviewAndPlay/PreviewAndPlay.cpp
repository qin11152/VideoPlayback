#include "PreviewAndPlay.h"
#include "module/utils/utils.h"

PreviewAndPlay::PreviewAndPlay()
	: PcmDatahandler(), YuvDataHandler()
{

}

PreviewAndPlay::~PreviewAndPlay()
{
	uninitModule();
}

int32_t PreviewAndPlay::initModule(const DataHandlerInitedInfo& info)
{
	m_uiConsumeThreadSleepTime = info.uiNeedSleepTime;
	m_uiPerFrameSampleCnt = info.uiPerFrameSampleCnt;
	m_bRunningState = true;
	m_ConsumeThread = std::thread(&PreviewAndPlay::handler, this);

	m_bInitState = true;
	return 0;
}

int32_t PreviewAndPlay::uninitModule()
{
	if (!m_bInitState)
	{
		return -1;
	}
	m_bRunningState = false;
	m_bInitState = false;
	m_PauseCV.notify_all();
	if (m_ptrQueueDecodedVideo)
	{
		m_ptrQueueDecodedVideo->uninitModule();
	}
	if (m_ptrPcmBuffer)
	{
		m_ptrPcmBuffer->unInitBuffer();
	}
	if (m_ConsumeThread.joinable())
	{
		m_ConsumeThread.join();
	}
	m_ptrPcmBuffer = nullptr;
	m_ptrQueueDecodedVideo = nullptr;
	return 0;
}

void PreviewAndPlay::setCallback(YuvCallBack callback)
{
	m_YuvCallback = callback;
}

void PreviewAndPlay::setCallback(AudioPlayCallback callback)
{
	m_AudioCallback = callback;
}

void PreviewAndPlay::pause()
{
	std::unique_lock<std::mutex> lck(m_PauseMutex);
	m_bPauseState = true;
}

void PreviewAndPlay::resume()
{
	std::unique_lock<std::mutex> lck(m_PauseMutex);
	m_bPauseState = false;
	m_PauseCV.notify_one();
}

void PreviewAndPlay::setFinishedCallback(YuvFinishedCallback callback)
{
	m_FinishedCallback = callback;
}

int32_t PreviewAndPlay::setVideoQueue(std::shared_ptr <MyPacketQueue<std::shared_ptr<DecodedImageInfo>>> ptrQueueDecodedVideo)
{
	m_ptrQueueDecodedVideo = ptrQueueDecodedVideo;
	return 0;
}

int32_t PreviewAndPlay::setAudioQueue(std::shared_ptr<Buffer> ptrPcmBuffer)
{
	m_ptrPcmBuffer = ptrPcmBuffer;
	return 0;
}

void PreviewAndPlay::setDecoderFinshedState(bool state)
{
	m_bDecoderFinished = state;
}

std::shared_ptr<YuvDataHandler> PreviewAndPlay::getYuvDataHandler()
{
	return std::dynamic_pointer_cast<YuvDataHandler>(shared_from_this());
}

std::shared_ptr<PcmDatahandler> PreviewAndPlay::getPcmDataHandler()
{
	return std::dynamic_pointer_cast<PcmDatahandler>(shared_from_this());
}

void PreviewAndPlay::handler()
{
	if (!m_bInitState)
	{
		return;
	}
	auto needPaintTime = std::chrono::system_clock::now();
	while (m_bRunningState && m_ptrQueueDecodedVideo->getSize() < 10)
	{
		continue;
	}
	static int cnt = 0;
	while (true)
	{
		{
			std::unique_lock<std::mutex> lck(m_PauseMutex);
			//暂停状态，等待解除暂停
			if (m_bPauseState)
			{
				m_PauseCV.wait(lck, [this]() {return !m_bRunningState || !m_bPauseState; });
				//每次暂停恢复之后都需要更新下此次paint的时间
				needPaintTime = std::chrono::system_clock::now();
			}
		}
		//解码后的视频队列和音频队列都为空，就等待解码线程解码
		//强制退出
		if (!m_bRunningState)
		{
			break;
		}

		if (m_bDecoderFinished)
		{
			if (m_FinishedCallback)
			{
				m_FinishedCallback();
			}
		}

		std::shared_ptr<DecodedImageInfo> videoInfo = nullptr;
		std::shared_ptr<AudioCallbackInfo> audioInfo = std::make_shared<AudioCallbackInfo>();

		if (m_ptrQueueDecodedVideo)
		{
			m_ptrQueueDecodedVideo->getPacket(videoInfo);
		}

		if (/*nullptr == audioInfo ||*/ nullptr == videoInfo)
		{
			continue;
		}

		//计算需要休眠多少
		auto currentTime = std::chrono::system_clock::now();
		auto diff = std::chrono::duration_cast<std::chrono::microseconds>(needPaintTime - currentTime).count();
		if (diff > 0)
		{
			//std::this_thread::sleep_for(std::chrono::microseconds(diff));
			utils::preciseSleep(std::chrono::duration_cast<std::chrono::microseconds>(needPaintTime - currentTime));
		}
		std::string t1 = utils::getTime(currentTime);
		std::string t2 = utils::getTime(needPaintTime);
		needPaintTime = needPaintTime + std::chrono::milliseconds(m_uiConsumeThreadSleepTime);
		std::string t = utils::getTime(needPaintTime);
		LOG_INFO("Need Paint Time:{},Get Frame Time{},Next Paint Time:{}", t2, t1, t);
		if (m_YuvCallback)
		{
			m_YuvCallback(videoInfo);
			cnt++;
		}
		if (m_AudioCallback)
		{
			if (m_ptrPcmBuffer)
			{
				int length = m_uiPerFrameSampleCnt * kOutputAudioChannels * av_get_bytes_per_sample((AVSampleFormat)kOutputAudioFormat);
				audioInfo->m_pPCMData = new uint8_t[length]{ 0 };
				if (m_ptrPcmBuffer->getBuffer(audioInfo->m_pPCMData, length))
				{
					audioInfo->m_ulPCMLength = length;
					//std::fstream fs("audio.pcm", std::ios::app | std::ios::binary);
					////把重采样之后的数据保存本地
					//fs.write((const char*)audioInfo->m_pPCMData, audioInfo->m_ulPCMLength);
					//fs.close();
					m_AudioCallback(audioInfo);
				}
			}
			//delete[]pcmData;
		}
		LOG_INFO("Consume End,Total Cnt:{}",cnt);
	}
}
