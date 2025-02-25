#include "AudioAndVideoOutput.h"

#include "module/utils/utils.h"

AudioAndVideoOutput::AudioAndVideoOutput()
{

}

AudioAndVideoOutput::~AudioAndVideoOutput()
{
	uninitModule();
}

int32_t AudioAndVideoOutput::initModule(const DataHandlerInitedInfo& info)
{
	m_bInitState = true;
	m_bRunningState = true;
	m_AudioThread = std::thread(&AudioAndVideoOutput::audio, this);
	m_VideoThread = std::thread(&AudioAndVideoOutput::video, this);

	return 0;
}

int32_t AudioAndVideoOutput::uninitModule()
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
	if (m_ptrQueueDecodedAudio)
	{
		m_ptrQueueDecodedAudio->uninitModule();
	}
	if (m_AudioThread.joinable())
	{
		m_AudioThread.join();
	}
	if (m_VideoThread.joinable())
	{
		m_VideoThread.join();
	}
	m_ptrQueueDecodedAudio = nullptr;
	m_ptrQueueDecodedVideo = nullptr;
	return 0;
}

void AudioAndVideoOutput::pause()
{
	std::unique_lock<std::mutex> lck(m_PauseMutex);
	m_bPauseState = true;
}

void AudioAndVideoOutput::resume()
{
	std::unique_lock<std::mutex> lck(m_PauseMutex);
	m_bPauseState = false;
	m_PauseCV.notify_all();
}

void AudioAndVideoOutput::seekTo(SeekParams params)
{
	m_dSeekTime = params.m_dSeekTime;
	m_bAudioSeekState = true;
	m_bVideoSeekState = true;
}

void AudioAndVideoOutput::nextFrame()
{
	if (m_ptrQueueDecodedVideo->getSize() <= 0)
	{
		return;
	}
	std::shared_ptr<DecodedImageInfo> videoInfo = nullptr;
	m_ptrQueueDecodedVideo->getPacket(videoInfo);
	qDebug() << "audio pts:" << m_ptrQueueDecodedAudio->front()->m_dPts << ",video pts:" << videoInfo->m_dPts;
	while (m_ptrQueueDecodedVideo->getSize() > 0 && m_ptrQueueDecodedAudio->front()->m_dPts < videoInfo->m_dPts)
	{
		m_ptrQueueDecodedAudio->pop_front();
	}
	if (m_YuvCallback)
	{
		m_YuvCallback(videoInfo);
	}
}

void AudioAndVideoOutput::renderPreviousFrame(const SeekParams& params)
{

}

void AudioAndVideoOutput::audio()
{
	qDebug() << "audio";
	if (!m_bInitState)
	{
		return;
	}
	int audioLength = m_uiPerFrameSampleCnt * kOutputAudioChannels * av_get_bytes_per_sample((AVSampleFormat)kOutputAudioFormat);
	auto startTime = std::chrono::high_resolution_clock::now();
	int64_t start_pts = AV_NOPTS_VALUE;
	while (true)
	{
		if (!m_bVideoReady)
		{
			continue;
		}
		{
			std::unique_lock<std::mutex> lck(m_PauseMutex);
			//暂停状态，等待解除暂停
			if (m_bPauseState)
			{
				m_PauseCV.wait(lck, [this]() {return !m_bRunningState || !m_bPauseState; });
			}
		}
		//解码后的视频队列和音频队列都为空，就等待解码线程解码
		//强制退出
		if (!m_bRunningState)
		{
			break;
		}
		std::shared_ptr<DecodedAudioInfo> audioInfo = nullptr;
		if (m_ptrQueueDecodedAudio)
		{
			m_ptrQueueDecodedAudio->getPacket(audioInfo);
		}
		if (nullptr == audioInfo)
		{
			continue;
		}
		if (!m_bAudioSeekState || ((audioInfo->m_dPts - m_dSeekTime) / max(std::abs(audioInfo->m_dPts), std::abs(m_dSeekTime)) >= -kdEpsilon))
		{
			m_bAudioSeekState = false;
		}
		else
		{
			continue;
		}
		int frame_size = audioInfo->m_uiNumberSamples; // 每帧样本数
		int sample_rate = audioInfo->m_uiSampleRate; // 采样率
		double frame_duration_ms = (frame_size * 1000.0) / sample_rate; // 持续时间（毫秒）
		m_dCurrentAduioPts = audioInfo->m_dPts;
#if 0
		if (start_pts == AV_NOPTS_VALUE) {
			// 初始化起始时间和起始 PTS
			startTime = std::chrono::high_resolution_clock::now();
			start_pts = audioInfo->m_dPts;
		}
		auto currentTime = std::chrono::high_resolution_clock::now();
		double target = audioInfo->m_dPts;
		double elapsed_time = std::chrono::duration<double>(currentTime - startTime).count();
		if (target > elapsed_time)
		{
			utils::preciseSleep((target - elapsed_time) * 1000);
			qDebug() << "target time" << target << ",elapsed time" << elapsed_time << ",sleep time" << ((target - elapsed_time) * 1000);
		}
		else {
			// 如果目标时间已过，说明帧过期，可以选择丢帧
		}
		if (m_AudioCallback)
		{
			m_AudioCallback(audioInfo);
		}
#endif
		if (m_AudioCallback)
		{
			m_AudioCallback(audioInfo);
		}
		utils::preciseSleep(frame_duration_ms);
	}
}

void AudioAndVideoOutput::video()
{
	if (!m_bInitState)
	{
		return;
	}
	int audioLength = m_uiPerFrameSampleCnt * kOutputAudioChannels * av_get_bytes_per_sample((AVSampleFormat)kOutputAudioFormat);
	while (true)
	{
		if (m_ptrQueueDecodedVideo->getSize() < 20 && !m_bVideoReady)
		{
			continue;
		}
		m_bVideoReady = true;
		{
			std::unique_lock<std::mutex> lck(m_PauseMutex);
			//暂停状态，等待解除暂停
			if (m_bPauseState)
			{
				m_PauseCV.wait(lck, [this]() {return !m_bRunningState || !m_bPauseState; });
			}
		}
		//解码后的视频队列和音频队列都为空，就等待解码线程解码
		//强制退出
		if (!m_bRunningState)
		{
			break;
		}
		std::shared_ptr<DecodedImageInfo> videoInfo = nullptr;
		if (m_ptrQueueDecodedVideo)
		{
			m_ptrQueueDecodedVideo->getPacket(videoInfo);
		}

		if (nullptr != videoInfo && (!m_bVideoSeekState || ((videoInfo->m_dPts - m_dSeekTime) / max(std::abs(videoInfo->m_dPts), std::abs(m_dSeekTime))) >= -kdEpsilon))
		{
			m_bAudioSeekState = false;
		}
		else
		{
			continue;
		}

		double diff = videoInfo->m_dPts - m_dCurrentAduioPts;
		if (diff > 0)
		{
			diff *= 1000;
			//if (diff > 18)
			//{
			//	diff *= 36;
			//}
			utils::preciseSleep(diff);
		}

		if (m_YuvCallback)
		{
			m_YuvCallback(videoInfo);
		}
	}
}
