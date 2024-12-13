#include "AtomDecoder.h"


AtomDecoder::AtomDecoder(std::vector<std::shared_ptr<VideoReader>> vecVideoReader)
	:m_vecVideoReader(vecVideoReader), formatContext(nullptr), videoCodecContext(nullptr)
{

}

AtomDecoder::~AtomDecoder()
{
	uninitModule();
}

int32_t AtomDecoder::initModule(const DecoderInitedInfo& info, DataHandlerInitedInfo& dataHandlerInfo)
{
	if (m_bRunningState)
	{
		return -1;
	}
	m_vecAudioFormatContext = info.vecAudioFormatContext;
	formatContext = info.atomVideoFormatContext;
	m_stuVideoInfo = info.outVideoInfo;
	m_stuAudioInfo = info.outAudioInfo;
	m_iVideoStreamIndex = info.iVideoIndex;

	if (0 != initAudioDecoder(info))
	{
		return -1;
	}

	if (0 != initVideoDecoder(info))
	{
		return - 1;
	}

	AVRational frameRate = info.atomVideoFormatContext->streams[m_iVideoStreamIndex]->avg_frame_rate;
	auto fps = av_q2d(frameRate);
	m_uiReadThreadSleepTime = (kmilliSecondsPerSecond / fps);
	m_uiPerFrameSampleCnt = m_stuAudioInfo.audioSampleRate / fps;
	m_dFrameDuration = av_q2d(info.atomVideoFormatContext->streams[m_iVideoStreamIndex]->time_base);

	dataHandlerInfo.uiNeedSleepTime = m_uiReadThreadSleepTime;
	dataHandlerInfo.uiPerFrameSampleCnt = m_uiPerFrameSampleCnt;

	m_ptrQueNeedDecodedVideoPacket = info.ptrAtomVideoPacketQueue;

	return (int32_t)ErrorCode::NoError;
}

int32_t AtomDecoder::uninitModule()
{
	m_bRunningState = false;
	for (auto& it : m_vecPCMBuffer)
	{
		for (auto item : *it)
		{
			item->unInitBuffer();
		}
	}
	for (auto& it : m_vecQueDecodedVideoPacket)
	{
		it->uninitModule();
	}
	if (m_ptrQueNeedDecodedVideoPacket)
	{
		m_ptrQueNeedDecodedVideoPacket->uninitModule();
	}
	for (auto iter : m_vecQueueNeedDecodedAudioPacket)
	{
		iter->uninitModule();
	}
	if (m_VideoDecodeThread.joinable())
	{
		m_VideoDecodeThread.join();
	}
	if (m_AudioDecodeThread.joinable())
	{
		m_AudioDecodeThread.join();
	}
	m_ptrQueNeedDecodedVideoPacket = nullptr;
	m_vecQueDecodedVideoPacket.clear();
	m_vecQueueNeedDecodedAudioPacket.clear();
	m_vecPCMBuffer.clear();
	m_iVideoStreamIndex = -1;
	if (swsContext)
	{
		sws_freeContext(swsContext);
		swsContext = nullptr;
	}
	for (auto iter : m_vecSwrContext)
	{
		if (iter != nullptr)
		{
			swr_free(&iter);
			iter = nullptr;
		}
	}
	m_vecSwrContext.clear();

	if (videoCodecContext)
	{
		avcodec_free_context(&videoCodecContext);
		videoCodecContext = nullptr;
	}
	for (auto iter : m_vecAudioCodecContext)
	{
		if (iter.first)
		{
			avcodec_free_context(&iter.first);
			iter.first = nullptr;
		}
	}
	m_vecAudioCodecContext.clear();

	m_bInitState = false;
	return 0;
}

int32_t AtomDecoder::addAtomVideoPacketQueue(std::shared_ptr<MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>> ptrPacketQueue)
{
	for (auto& item : m_vecQueDecodedVideoPacket)
	{
		if (item == ptrPacketQueue)
		{
			return -1;
		}
	}
	m_vecQueDecodedVideoPacket.push_back(ptrPacketQueue);
	return 0;
}

int32_t AtomDecoder::addAtomAudioPacketQueue(std::shared_ptr<std::vector<std::shared_ptr<Buffer>>> vecBuffer)
{
	for (auto& item : m_vecPCMBuffer)
	{
		if (item == vecBuffer)
		{
			return -1;
		}
	}
	m_vecPCMBuffer.push_back(vecBuffer);
	return 0;
}

int32_t AtomDecoder::seekTo(double_t seekTime)
{
	return 0;
}

void AtomDecoder::registerFinishedCallback(DecoderFinishedCallback callback)
{
	m_finishedCallback = callback;
}

int32_t AtomDecoder::initVideoDecoder(const DecoderInitedInfo& info)
{
	if (nullptr == info.atomVideoCodecParameters)
	{
		return -1;
	}
	videoCodecContext = avcodec_alloc_context3(info.videoCodec);

	if (AV_HWDEVICE_TYPE_NONE != info.m_eDeviceType)
	{
		AVBufferRef* hwDeviceCtx = nullptr;
		int err = av_hwdevice_ctx_create(&hwDeviceCtx, info.m_eDeviceType, nullptr, nullptr, 0);
		if (err < 0)
		{
			avcodec_free_context(&videoCodecContext);
			return (int32_t)ErrorCode::AllocateContextError;
		}

		// 设置硬件设备上下文
		videoCodecContext->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
		av_buffer_unref(&hwDeviceCtx);
	}

	videoCodecContext->thread_count = 16; // 根据实际 CPU 核心数调整
	videoCodecContext->thread_type = FF_THREAD_FRAME; // 按帧并行解码

	videoCodecContext->flags |= AVFMT_FLAG_GENPTS;

	if (avcodec_parameters_to_context(videoCodecContext, info.atomVideoCodecParameters) < 0)
	{
		avcodec_free_context(&videoCodecContext);
		return (int32_t)ErrorCode::AllocateContextError;
	}

	// 打开解码器
	if (avcodec_open2(videoCodecContext, info.atomVideoCodec, nullptr) < 0)
	{
		avcodec_free_context(&videoCodecContext);
		return (int32_t)ErrorCode::AllocateContextError;
	}

	swsContext = sws_getContext(
		videoCodecContext->width, videoCodecContext->height, videoCodecContext->pix_fmt,
		m_stuVideoInfo.width, m_stuVideoInfo.height, m_stuVideoInfo.videoFormat,
		SWS_BILINEAR, nullptr, nullptr, nullptr);
	return 0;
}

int32_t AtomDecoder::initAudioDecoder(const DecoderInitedInfo& info)
{
	if (info.vecAtomAudioCodec.size() != info.vecAtomAudioCodecParameters.size())
	{
		return -1;
	}
	for (int i = 0; i < info.vecAtomAudioCodec.size(); ++i)
	{
		auto audioCodecContextTmp = avcodec_alloc_context3(info.vecAtomAudioCodec[i]);
		avcodec_parameters_to_context(audioCodecContextTmp, info.vecAtomAudioCodecParameters[i].first);
		avcodec_open2(audioCodecContextTmp, info.vecAtomAudioCodec[i], nullptr);
		auto swrContextTmp = swr_alloc();
		if (!swrContextTmp)
		{
			return (int32_t)ErrorCode::AllocateRsampleError;
		}
		AVChannelLayout in_channel_layout;
		av_channel_layout_default(&in_channel_layout, audioCodecContextTmp->channels);
		AVChannelLayout out_channel_layout;
		av_channel_layout_default(&out_channel_layout, m_stuAudioInfo.audioChannels); // Stereo output
		int out_sample_rate = m_stuAudioInfo.audioSampleRate;
		// int out_sample_rate = audioCodecContext->sample_rate;
		AVSampleFormat out_sample_fmt = m_stuAudioInfo.audioFormat;
		if (swr_alloc_set_opts2(&swrContextTmp, &out_channel_layout, out_sample_fmt, out_sample_rate,
			&in_channel_layout, audioCodecContextTmp->sample_fmt, audioCodecContextTmp->sample_rate, 0, nullptr) < 0)
		{
			swr_free(&swrContextTmp);
			return (int32_t)ErrorCode::AllocateRsampleError;
		}
		if (swr_init(swrContextTmp) < 0)
		{
			swr_free(&swrContextTmp);
			return (int32_t)ErrorCode::AllocateRsampleError;
		}
		m_vecSwrContext.push_back(swrContextTmp);
		m_vecAudioCodecContext.push_back({ audioCodecContextTmp, info.vecAtomAudioCodecParameters[i].second });
	}
	return 0;

}

void AtomDecoder::readVideoPacket()
{
	if (!m_bInitState)
	{
		return;
	}
	while (true)
	{
		if (!m_bRunningState)
		{
			break;
		}
		{
			std::unique_lock<std::mutex> lck(m_PauseMutex);
			if (m_bPauseState)
			{
				m_PauseCV.wait(lck, [this]() {return !m_bRunningState || !m_bPauseState; });
			}
		}
		//if (m_ptrVideoReader->getFinishedState() && 0 == m_ptrQueNeedDecodedPacket->getSize())
		//{
		//	if (!m_bDecoderedFinished)
		//	{
		//		flushDecoder();
		//		m_bDecoderedFinished = true;
		//		if (m_finishedCallback)
		//		{
		//			m_finishedCallback();
		//		}
		//	}
		//	std::this_thread::sleep_for(std::chrono::milliseconds(100));
		//	continue;
		//}
		std::shared_ptr<PacketWaitDecoded> packet = nullptr;
		if (m_ptrQueNeedDecodedVideoPacket)
		{
			m_ptrQueNeedDecodedVideoPacket->getPacket(packet);
		}
		if (packet)
		{
			LOG_INFO("Get One Packet");
			auto start = std::chrono::steady_clock::now();
			decodeVideo(packet);
			auto end = std::chrono::steady_clock::now();
			//printf("decoder time:%lld\n", std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
		}
		LOG_INFO("Decoder End");
	}
}

void AtomDecoder::readAudioPacket()
{
	if (!m_bInitState)
	{
		return;
	}
	while (true)
	{
		if (!m_bRunningState)
		{
			break;
		}
		{
			std::unique_lock<std::mutex> lck(m_PauseMutex);
			if (m_bPauseState)
			{
				m_PauseCV.wait(lck, [this]() {return !m_bRunningState || !m_bPauseState; });
			}
		}
		//if (m_ptrVideoReader->getFinishedState() && 0 == m_ptrQueNeedDecodedPacket->getSize())
		//{
		//	if (!m_bDecoderedFinished)
		//	{
		//		flushDecoder();
		//		m_bDecoderedFinished = true;
		//		if (m_finishedCallback)
		//		{
		//			m_finishedCallback();
		//		}
		//	}
		//	std::this_thread::sleep_for(std::chrono::milliseconds(100));
		//	continue;
		//}
		std::shared_ptr<PacketWaitDecoded> packet = nullptr;
		for (int i = 0; i < m_vecQueueNeedDecodedAudioPacket.size(); ++i)
		{
			if (m_vecQueueNeedDecodedAudioPacket[i])
			{
				m_vecQueueNeedDecodedAudioPacket[i]->getPacket(packet);
				if (packet)
				{
					decodeAudio(packet, i);
				}
			}
			LOG_INFO("Decoder End");
		}
	}
}

void AtomDecoder::decodeVideo(std::shared_ptr<PacketWaitDecoded> packet)
{
	AVFrame* frame = av_frame_alloc();
	AVFrame* swFrame = av_frame_alloc(); // 用于硬件帧转换
	int ret = 0;
	if (!packet)
	{
		return;
	}
	if (avcodec_send_packet(videoCodecContext, packet->packet) == 0)
	{
		int ret = avcodec_receive_frame(videoCodecContext, frame);
		if (ret < 0)
		{
			LOG_ERROR("avcodec_receive_frame error");
		}
		while (ret >= 0)
		{
			double pts = frame->pts * m_dFrameDuration;
			std::shared_ptr<VideoCallbackInfo> videoInfo = std::make_shared<VideoCallbackInfo>();
			videoInfo->width = videoCodecContext->width;
			videoInfo->height = videoCodecContext->height;
			videoInfo->videoFormat = videoCodecContext->pix_fmt;
			videoInfo->m_dPts = pts;
			// 如果是硬件帧，需要转换到系统内存
			if (frame->hw_frames_ctx)
			{
				swFrame = av_frame_alloc();
				auto transfer_start = std::chrono::steady_clock::now();
				// 将硬件帧转换为软件帧
				if (av_hwframe_transfer_data(swFrame, frame, 0) < 0)
				{
					av_frame_free(&frame);
					av_frame_free(&swFrame);
					return;
				}
				auto transfer_end = std::chrono::steady_clock::now();
				std::shared_ptr<VideoCallbackInfo> videoInfo = nullptr;
				// 转换为目标格式
				if (!swsContext)
				{
					swsContext = sws_getContext(
						videoCodecContext->width, videoCodecContext->height, (AVPixelFormat)swFrame->format,
						m_stuVideoInfo.width, m_stuVideoInfo.height, m_stuVideoInfo.videoFormat,
						SWS_BILINEAR, nullptr, nullptr, nullptr);
				}
				{
					auto convert_start = std::chrono::steady_clock::now();
					AVFrame* yuvFrame = av_frame_alloc();
					av_image_alloc(yuvFrame->data, yuvFrame->linesize, m_stuVideoInfo.width, m_stuVideoInfo.height, m_stuVideoInfo.videoFormat, 1);

					sws_scale(swsContext, swFrame->data, swFrame->linesize, 0, videoCodecContext->height, yuvFrame->data, yuvFrame->linesize);
					auto convert_end = std::chrono::steady_clock::now();
					//printf("decoder time:%lld,transfer time: %lld, convert time: %lld\n", std::chrono::duration_cast<std::chrono::milliseconds>(decode_end-decode_start).count(), std::chrono::duration_cast<std::chrono::milliseconds>(transfer_end - transfer_start).count(), std::chrono::duration_cast<std::chrono::milliseconds>(convert_end - convert_start).count());
					LOG_INFO("Video Decoder Convert");
					videoInfo->videoFormat = m_stuVideoInfo.videoFormat;
					videoInfo->width = m_stuVideoInfo.width;
					videoInfo->height = m_stuVideoInfo.height;
					videoInfo->dataSize = m_stuVideoInfo.width * m_stuVideoInfo.height * 2;
					videoInfo->yuvData = new uint8_t[m_stuVideoInfo.width * m_stuVideoInfo.height * 2];
					memcpy(videoInfo->yuvData, yuvFrame->data[0], m_stuVideoInfo.width * m_stuVideoInfo.height * 2);
					av_freep(yuvFrame->data);
					av_frame_free(&yuvFrame);
				}

				// 释放
				av_frame_free(&swFrame);
			}
			else
			{
				if (!swsContext)
				{
					swsContext = sws_getContext(
						videoCodecContext->width, videoCodecContext->height, (AVPixelFormat)frame->format,
						m_stuVideoInfo.width, m_stuVideoInfo.height, m_stuVideoInfo.videoFormat,
						SWS_BILINEAR, nullptr, nullptr, nullptr);
				}
				auto convert_start = std::chrono::steady_clock::now();
				AVFrame* yuvFrame = av_frame_alloc();
				av_image_alloc(yuvFrame->data, yuvFrame->linesize, m_stuVideoInfo.width, m_stuVideoInfo.height, m_stuVideoInfo.videoFormat, 1);

				sws_scale(swsContext, frame->data, frame->linesize, 0, videoCodecContext->height, yuvFrame->data, yuvFrame->linesize);
				auto convert_end = std::chrono::steady_clock::now();
				//printf("decoder time:%lld, convert time: %lld\n", std::chrono::duration_cast<std::chrono::milliseconds>(decode_end - decode_start).count(), std::chrono::duration_cast<std::chrono::milliseconds>(convert_end - convert_start).count());
				LOG_INFO("Video Decoder Convert");
				videoInfo->videoFormat = m_stuVideoInfo.videoFormat;
				videoInfo->width = m_stuVideoInfo.width;
				videoInfo->height = m_stuVideoInfo.height;
				videoInfo->dataSize = m_stuVideoInfo.width * m_stuVideoInfo.height * 2;
				videoInfo->yuvData = new uint8_t[m_stuVideoInfo.width * m_stuVideoInfo.height * 2];
				memcpy(videoInfo->yuvData, yuvFrame->data[0], m_stuVideoInfo.width * m_stuVideoInfo.height * 2);
				av_freep(yuvFrame->data);
				av_frame_free(&yuvFrame);
			}
			for (auto& it : m_vecQueDecodedVideoPacket)
			{
				it->addPacket(videoInfo);
			}
			ret = avcodec_receive_frame(videoCodecContext, frame);
		}
	}
	else
	{
		LOG_ERROR("video avcodec_send_packet error");
	}
	av_frame_free(&frame);
}

void AtomDecoder::decodeAudio(std::shared_ptr<PacketWaitDecoded> packet, int index)
{
	AVFrame* frame = av_frame_alloc();
	int swr_size = 0;
	int resampled_linesize;
	int max_resampled_samples = 0;
	uint8_t** resampled_data = nullptr;
	if (!packet)
	{
		return;
	}
	if (avcodec_send_packet(m_vecAudioCodecContext[index].first, packet->packet) == 0)
	{
		while (avcodec_receive_frame(m_vecAudioCodecContext[index].first, frame) == 0)
		{
			LOG_INFO("Audio Decoder Begin Handle");
			//std::fstream fs("audio.pcm", std::ios::app | std::ios::binary);
			////把重采样之前的数据保存本地
			//fs.write((const char *)frame->data[0], frame->linesize[0]);
			//fs.close();

			int resampled_samples = av_rescale_rnd(
				swr_get_delay(m_vecSwrContext[index], m_vecAudioCodecContext[index].first->sample_rate) + frame->nb_samples,
				m_stuAudioInfo.audioSampleRate, m_vecAudioCodecContext[index].first->sample_rate, AV_ROUND_UP);

			if (resampled_samples > max_resampled_samples)
			{
				if (resampled_data)
				{
					av_freep(&resampled_data[0]);
				}
				av_samples_alloc_array_and_samples(&resampled_data, &resampled_linesize,
					kOutputAudioChannels, resampled_samples,
					m_stuAudioInfo.audioFormat, 0);
				max_resampled_samples = resampled_samples;
			}

			int converted_samples = swr_convert(m_vecSwrContext[index], resampled_data, resampled_samples,
				(const uint8_t**)frame->data, frame->nb_samples);

			int pcmNumber = converted_samples * kOutputAudioChannels * av_get_bytes_per_sample(m_stuAudioInfo.audioFormat);

			//std::fstream fs("audio0.pcm", std::ios::app | std::ios::binary);
			////把重采样之后的数据保存本地
			//fs.write((const char*)resampled_data[0], pcmNumber);
			//fs.close();

			if (resampled_data)
			{
				for (auto& it : m_vecPCMBuffer)
				{
					(*it)[index]->appendData(resampled_data[0], pcmNumber);
				}
			}
		}
	}
	LOG_INFO("Decoder Audio End");
	av_frame_free(&frame);
	if (resampled_data)
	{
		av_freep(&resampled_data[0]);
	}
}
