#include "AudioAndVideoOutput.h"

#include "module/utils/utils.h"
#include "module/source/LocalFileSource.h"

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

	m_uiPerFrameSampleCnt = info.uiPerFrameSampleCnt;

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
	if (m_bPauseState)
	{
		return;
	}
	m_bPauseState = true;
}

void AudioAndVideoOutput::resume()
{
	std::unique_lock<std::mutex> lck(m_PauseMutex);
	m_bPauseState = false;
	m_PauseCV.notify_all();
}

void AudioAndVideoOutput::seekTo(const SeekParams& params)
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
	//qDebug() << "audio pts:" << m_ptrQueueDecodedAudio->front()->m_dPts << ",video pts:" << videoInfo->m_dPts;
	while (m_ptrQueueDecodedVideo->getSize() > 0 && m_ptrQueueDecodedAudio->front()->m_dPts < videoInfo->m_dPts)
	{
		m_ptrQueueDecodedAudio->pop_front();
	}
	if (m_YuvCallback)
	{
		m_dCurrentVideoPts = videoInfo->m_dPts;
		m_YuvCallback(videoInfo);
	}
}

void AudioAndVideoOutput::previousFrame(const SeekParams& params)
{
	//直到拿到目标帧
	while (true)
	{
		std::shared_ptr<DecodedImageInfo> videoInfo = nullptr;
		m_ptrQueueDecodedVideo->getPacket(videoInfo);
		qDebug() << "get packet pts" << videoInfo->m_dPts;
		//if ((videoInfo->m_dPts - params.m_dDstPts) / max(std::abs(videoInfo->m_dPts), std::abs(params.m_dDstPts)) >= -kdEpsilon)
		if (std::fabs(videoInfo->m_dPts - params.m_dDstPts) <= kdEpsilon)
		{
			//处理异常情况，解码器有可能残留之前的帧，此时dts大于目标dts，但是diff很大
			//if ((videoInfo->m_dPts - params.m_dDstPts) > 0.5)
			//{
			//	continue;
			//}
			if (m_YuvCallback)
			{
				qDebug() << "render previous frame:" << videoInfo->m_dPts << ",dst pts:" << params.m_dDstPts;
				m_dCurrentVideoPts = videoInfo->m_dPts;
				m_YuvCallback(videoInfo);
			}
			break;
		}

		std::shared_ptr<DecodedAudioInfo> audioInfo = nullptr;
		m_ptrQueueDecodedAudio->getPacket(audioInfo);
		//小于时间戳的抛弃
		qDebug() << "get audio packet pts" << audioInfo->m_dPts;
		if (audioInfo->m_dPts - params.m_dDstPts > kdEpsilon)
		{
			//异常帧抛弃
			if ((audioInfo->m_dPts - params.m_dDstPts) > 0.1)
			{
				continue;
			}
			//到了目标，视频还没到，加回去
			m_ptrQueueDecodedAudio->addPacket(audioInfo);
		}
	}
	while (true)
	{
		std::shared_ptr<DecodedAudioInfo> audioInfo = nullptr;
		m_ptrQueueDecodedAudio->getPacket(audioInfo);
#ifndef  WIN32
		if ((audioInfo->m_dPts - params.m_dDstPts) / std::max(std::abs(audioInfo->m_dPts), std::abs(params.m_dDstPts)) >= -kdEpsilon)
#else
		if ((audioInfo->m_dPts - params.m_dDstPts) / max(std::abs(audioInfo->m_dPts), std::abs(params.m_dDstPts)) >= -kdEpsilon)
#endif // ! WIN32
		{
			if ((audioInfo->m_dPts - params.m_dDstPts) > 0.1)
			{
				continue;
			}
			m_ptrQueueDecodedAudio->addPacket(audioInfo);
			break;
		}
	}

}

void AudioAndVideoOutput::audio()
{
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

		if (LocalFileSource::getDecoderFinishState() && m_ptrQueueDecodedAudio->getSize() <= 0)
		{
			if (!m_bAudioConsumeFinished)
			{
				m_bAudioConsumeFinished = true;
				consumeFinished();
			}
			continue;
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
		auto audio_callback_time = av_gettime_relative();
#ifndef  WIN32
		if (!m_bAudioSeekState || ((audioInfo->m_dPts - m_dSeekTime) / std::max(std::abs(audioInfo->m_dPts), std::abs(m_dSeekTime)) >= -kdEpsilon))
#else
		if (!m_bAudioSeekState || ((audioInfo->m_dPts - m_dSeekTime) / max(std::abs(audioInfo->m_dPts), std::abs(m_dSeekTime)) >= -kdEpsilon))
#endif // ! WIN32
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
			//qDebug() << "render audio pts:" << audioInfo->m_dPts;
			m_AudioCallback(audioInfo);
		}
		m_stuAudioClock.setClock(audioInfo->m_dPts, audio_callback_time / 1000000.0);
		utils::preciseSleep(frame_duration_ms, true);
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
		if (LocalFileSource::getDecoderFinishState() && m_ptrQueueDecodedVideo->getSize() <= 0)
		{
			if (!m_bVideoConsumeFinished)
			{
				m_bVideoConsumeFinished = true;
				consumeFinished();
			}
			continue;
		}
		std::shared_ptr<DecodedImageInfo> videoInfo = nullptr;
		if (m_ptrQueueDecodedVideo)
		{
			m_ptrQueueDecodedVideo->getPacket(videoInfo);
			printf("video size:%d\n", m_ptrQueueDecodedVideo->getSize());
		}
#ifndef WIN32
		if (nullptr != videoInfo && (!m_bVideoSeekState || ((videoInfo->m_dPts - m_dSeekTime) / std::max(std::abs(videoInfo->m_dPts), std::abs(m_dSeekTime))) >= -kdEpsilon))
#else
		if (nullptr != videoInfo && (!m_bVideoSeekState || ((videoInfo->m_dPts - m_dSeekTime) / max(std::abs(videoInfo->m_dPts), std::abs(m_dSeekTime))) >= -kdEpsilon))
#endif // !WIN32
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
			//qDebug() << "video pts" << videoInfo->m_dPts << "audio pts" << m_dCurrentAduioPts << "diff" << diff;
			diff *= 1000;
			//if (diff > 18)
			//{
			//	diff *= 36;
			//}
			utils::preciseSleep(diff);
		}

		if (m_YuvCallback)
		{
			//qDebug() << "render video pts:" << videoInfo->m_dPts;
			m_dCurrentVideoPts = videoInfo->m_dPts;
			m_YuvCallback(videoInfo);
		}
	}
}


void AudioAndVideoOutput::consumeFinished()
{
	if (!m_bVideoConsumeFinished || !m_bAudioConsumeFinished)
	{
		return;
	}
	if (m_FinishedCallback)
	{
		m_FinishedCallback();
	}
}
