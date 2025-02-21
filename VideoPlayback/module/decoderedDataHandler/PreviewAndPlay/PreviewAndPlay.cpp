#define NOMINMAX
#include "PreviewAndPlay.h"
#include "module/utils/utils.h"
#include "module/source/LocalFileSource.h"

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

void PreviewAndPlay::seekTo(SeekParams params)
{
	m_bSeekState = true;
	m_dSeekTime = params.m_dSeekTime;
}

void PreviewAndPlay::nextFrame()
{
	pause();
	if (m_ptrQueueDecodedVideo->getSize() <= 0)
	{
		return;
	}
	std::shared_ptr<DecodedImageInfo> videoInfo = nullptr;
	m_ptrQueueDecodedVideo->getPacket(videoInfo);

	if (m_YuvCallback)
	{
		m_YuvCallback(videoInfo);
		m_dCurrentVideoPts = videoInfo->m_dPts;
	}

	int length = m_uiPerFrameSampleCnt * kOutputAudioChannels * av_get_bytes_per_sample((AVSampleFormat)kOutputAudioFormat);
	auto ptrPCMData = new uint8_t[length]{ 0 };
	m_ptrPcmBuffer->getBuffer(ptrPCMData, length);
	delete[]ptrPCMData;
}

void PreviewAndPlay::renderPreviousFrame(const SeekParams& params)
{
	m_bSeekState = true;
	m_dSeekTime = params.m_dDstPts;
	std::shared_ptr<DecodedImageInfo> videoInfo = nullptr;
	int audioLength = m_uiPerFrameSampleCnt * kOutputAudioChannels * av_get_bytes_per_sample((AVSampleFormat)kOutputAudioFormat);
	while(true)
	{ 
		m_ptrQueueDecodedVideo->getPacket(videoInfo);
		if (params.direction<0 && videoInfo->m_dPts>m_dSeekTime)
		{
			qDebug() << "old frame";
			continue;
		}
		if (!m_bSeekState || ((videoInfo->m_dPts - m_dSeekTime) / std::max(std::abs(videoInfo->m_dPts), std::abs(m_dSeekTime)) >= -kdEpsilon))
		{
			qDebug() << "seek to frame pts" << videoInfo->m_dPts;
			m_ptrQueueDecodedVideo->addPacket(videoInfo);
			m_bSeekState = false; // 如果进入了这个分支，说明已经到达了目标点，重置状态
		}
		else
		{
			qDebug() << "drop frame pts:" << videoInfo->m_dPts;
			m_ulDropVideoFrameCntAfterSeek++;
			continue;
		}
		while (m_ulDropVideoFrameCntAfterSeek > 0)
		{
			if (audioLength < m_ptrPcmBuffer->getBufferSize())
			{
				m_ptrPcmBuffer->popFromTop(audioLength);
				m_ulDropVideoFrameCntAfterSeek--;
			}
			else
			{
				break;
			}
		}

		if (m_ulDropVideoFrameCntAfterSeek > 0)
		{
			continue;
		}
		else
		{
			break;
		}
	}
	m_ptrQueueDecodedVideo->getPacket(videoInfo);
	m_YuvCallback(videoInfo);
	m_dCurrentVideoPts = videoInfo->m_dPts;
	m_ptrQueueDecodedVideo->addPacket(videoInfo);
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
	int audioLength = m_uiPerFrameSampleCnt * kOutputAudioChannels * av_get_bytes_per_sample((AVSampleFormat)kOutputAudioFormat);
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

		if (LocalFileSource::getDecoderFinishState())
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

		//视频为空或音频数量不足的时候，继续等待
		if (/*nullptr == audioInfo ||*/ nullptr == videoInfo)
		{
			continue;
		}

		if (!m_bSeekState || ((videoInfo->m_dPts - m_dSeekTime) / std::max(std::abs(videoInfo->m_dPts), std::abs(m_dSeekTime)) >= -kdEpsilon))
		{
			m_bSeekState = false; // 如果进入了这个分支，说明已经到达了目标点，重置状态
		}
		else
		{
			m_ulDropVideoFrameCntAfterSeek++;
			continue;
		}

		while (m_ulDropVideoFrameCntAfterSeek > 0)
		{
			if (audioLength < m_ptrPcmBuffer->getBufferSize())
			{
				m_ptrPcmBuffer->popFromTop(audioLength);
				m_ulDropVideoFrameCntAfterSeek--;
			}
			else
			{
				break;
			}
		}

		if (m_ulDropVideoFrameCntAfterSeek > 0 || m_ptrPcmBuffer->getBufferSize() < audioLength)
		{
			m_ptrQueueDecodedVideo->addPacket(videoInfo);
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
			m_dCurrentVideoPts = videoInfo->m_dPts;
			cnt++;
		}
		if (m_AudioCallback)
		{
			if (m_ptrPcmBuffer)
			{
				audioInfo->m_pPCMData = new uint8_t[audioLength]{ 0 };
				if (m_ptrPcmBuffer->getBuffer(audioInfo->m_pPCMData, audioLength))
				{
					audioInfo->m_ulPCMLength = audioLength;
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
