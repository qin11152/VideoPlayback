#include "HardDecoder.h"

HardDecoder::HardDecoder(std::shared_ptr<VideoReader> ptrVideoReader)
	: m_ptrVideoReader(ptrVideoReader), videoCodecContext(nullptr), audioCodecContext(nullptr),
	m_iVideoStreamIndex(-1), m_iAudioStreamIndex(-1)
{

}

HardDecoder::~HardDecoder()
{
	uninitModule();
}

int32_t HardDecoder::initModule(const DecoderInitedInfo& info, DataHandlerInitedInfo& dataHandlerInfo)
{
	if (m_bInitState)
	{
		return -1;
	}
	if (nullptr == info.formatContext)
	{
		return -1;
	}

	m_stuAudioInfo = info.outAudioInfo;
	m_stuVideoInfo = info.outVideoInfo;
	m_iVideoStreamIndex = info.iVideoIndex;
	m_iAudioStreamIndex = info.iAudioIndex;
	fileFormat = info.formatContext;

	if(0 != initVideoDecoder(info))
	{
		return -1;
	}

	if (0 != initAudioDecoder(info) && -1 != info.iAudioIndex)
	{
		return -1;
	}
	
	AVRational frameRate = info.formatContext->streams[m_iVideoStreamIndex]->avg_frame_rate;
	auto fps = av_q2d(frameRate);
	m_uiReadThreadSleepTime = (kmilliSecondsPerSecond / fps);
	m_uiPerFrameSampleCnt = m_stuAudioInfo.audioSampleRate / fps;
	m_dFrameDuration = av_q2d(info.formatContext->streams[m_iVideoStreamIndex]->time_base);

	dataHandlerInfo.uiNeedSleepTime = m_uiReadThreadSleepTime;
	dataHandlerInfo.uiPerFrameSampleCnt = m_uiPerFrameSampleCnt;

	m_ptrQueNeedDecodedPacket = info.ptrPacketQueue;

	m_bInitState = true;
	m_bRunningState = true;

	m_DecoderThread = std::thread(&HardDecoder::decode, this);
	return 0;

}

int32_t HardDecoder::uninitModule()
{
	m_bRunningState = false;
	for (auto& it : m_vecPCMBufferPtr)
	{
		it->unInitBuffer();
	}
	for (auto& it : m_vecQueDecodedPacket)
	{
		it->uninitModule();
	}
	if (m_ptrQueNeedDecodedPacket)
	{
		m_ptrQueNeedDecodedPacket->uninitModule();
	}
	if (m_DecoderThread.joinable())
	{
		m_DecoderThread.join();
	}
	m_vecQueDecodedPacket.clear();
	m_vecPCMBufferPtr.clear();
	m_iAudioStreamIndex = -1;
	m_iVideoStreamIndex = -1;
	if (swsContext)
	{
		sws_freeContext(swsContext);
		swsContext = nullptr;
	}
	if (swrContext)
	{
		swr_free(&swrContext);
		swrContext = nullptr;
	}

	if (videoCodecContext)
	{
		avcodec_free_context(&videoCodecContext);
		videoCodecContext = nullptr;
	}
	if (audioCodecContext)
	{
		avcodec_free_context(&audioCodecContext);
		audioCodecContext = nullptr;
	}
	m_bInitState = false;
	return 0;
}

int32_t HardDecoder::addPCMBuffer(std::shared_ptr<Buffer> ptrPCMBuffer)
{
	std::unique_lock<std::mutex> lck(m_PcmBufferAddMutex);
	if (std::find(m_vecPCMBufferPtr.begin(), m_vecPCMBufferPtr.end(), ptrPCMBuffer) == m_vecPCMBufferPtr.end())
	{
		m_vecPCMBufferPtr.push_back(ptrPCMBuffer);
	}
	else
	{
		return -1;
	}
	return 0;
}

int32_t HardDecoder::addPacketQueue(std::shared_ptr<MyPacketQueue<std::shared_ptr<VideoCallbackInfo>>> ptrPacketQueue)
{
	std::unique_lock<std::mutex> lck(m_VideoQueueAddMutex);
	if (std::find(m_vecQueDecodedPacket.begin(), m_vecQueDecodedPacket.end(), ptrPacketQueue) == m_vecQueDecodedPacket.end())
	{
		m_vecQueDecodedPacket.push_back(ptrPacketQueue);
	}
	else
	{
		return -1;
	}
	return 0;
}

int32_t HardDecoder::seekTo(double_t seekTime)
{
	//正在快进或者快退，不处理
	if (m_bSeekState)
	{
		return -1;
	}
	//未初始化的时候不处理
	if (!m_bInitState)
	{
		return -2;
	}
	if (seekTime < 0 || seekTime > fileFormat->duration)
	{
		return -3;
	}
	m_dSeekTime = seekTime;
	m_bSeekState = true;
	ThreadPool::get_mutable_instance().submit(std::bind(&HardDecoder::seekOperate, this));
	return 0;
}

void HardDecoder::registerFinishedCallback(DecoderFinishedCallback callback)
{
	m_finishedCallback = callback;
}

int32_t HardDecoder::initVideoDecoder(const DecoderInitedInfo& info)
{
	if (nullptr == info.videoCodecParameters)
	{
		return -1;
	}
	videoCodecContext = avcodec_alloc_context3(info.videoCodec);

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

	videoCodecContext->thread_count = 16; // 根据实际 CPU 核心数调整
	videoCodecContext->thread_type = FF_THREAD_FRAME; // 按帧并行解码

	videoCodecContext->flags |= AVFMT_FLAG_GENPTS;

	if (avcodec_parameters_to_context(videoCodecContext, info.videoCodecParameters) < 0)
	{
		avcodec_free_context(&videoCodecContext);
		return (int32_t)ErrorCode::AllocateContextError;
	}

	// 打开解码器
	if (avcodec_open2(videoCodecContext, info.videoCodec, nullptr) < 0) 
	{
		avcodec_free_context(&videoCodecContext);
		return (int32_t)ErrorCode::AllocateContextError;
	}
	printf("Using decoder: %s\n", videoCodecContext->codec->name);
	swsContext = sws_getContext(
		videoCodecContext->width, videoCodecContext->height, videoCodecContext->pix_fmt,
		m_stuVideoInfo.width, m_stuVideoInfo.height, m_stuVideoInfo.videoFormat,
		SWS_BILINEAR, nullptr, nullptr, nullptr);
	return 0;
}

int32_t HardDecoder::initAudioDecoder(const DecoderInitedInfo& info)
{
	if(nullptr==info.audioCodec||nullptr==info.audioCodecParameters)
	{
		return -1;
	}
	audioCodecContext = avcodec_alloc_context3(info.audioCodec);
	avcodec_parameters_to_context(audioCodecContext, info.audioCodecParameters);
	avcodec_open2(audioCodecContext, info.audioCodec, nullptr);

	swrContext = swr_alloc();
	if (!swrContext)
	{
		return (int32_t)ErrorCode::AllocateRsampleError;
	}

	AVChannelLayout in_channel_layout;
	av_channel_layout_default(&in_channel_layout, audioCodecContext->ch_layout.nb_channels);

	AVChannelLayout out_channel_layout;
	av_channel_layout_default(&out_channel_layout, m_stuAudioInfo.audioChannels); // Stereo output

	int out_sample_rate = m_stuAudioInfo.audioSampleRate;
	// int out_sample_rate = audioCodecContext->sample_rate;
	AVSampleFormat out_sample_fmt = m_stuAudioInfo.audioFormat;

	if (swr_alloc_set_opts2(&swrContext, &out_channel_layout, out_sample_fmt, out_sample_rate,
		&in_channel_layout, audioCodecContext->sample_fmt, audioCodecContext->sample_rate, 0, nullptr) < 0)
	{
		swr_free(&swrContext);
		return (int32_t)ErrorCode::AllocateRsampleError;
	}

	if (swr_init(swrContext) < 0)
	{
		swr_free(&swrContext);
		return (int32_t)ErrorCode::AllocateRsampleError;
	}
	return 0;
}

void HardDecoder::flushDecoder()
{
	AVFrame* frame = av_frame_alloc();
	AVFrame* swFrame = nullptr;
	avcodec_send_packet(videoCodecContext, NULL);

	// 继续读取所有缓冲区中的帧
	while (avcodec_receive_frame(videoCodecContext, frame) >= 0)
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
		for (auto& it : m_vecQueDecodedPacket)
		{
			it->addPacket(videoInfo);
		}
		// 处理frame
	}
}

void HardDecoder::seekOperate()
{
	m_ptrVideoReader->pause();
	m_bPauseState = true;

	m_ptrQueNeedDecodedPacket->clearQueue();
	for (auto iter : m_vecQueDecodedPacket)
	{
		iter->clearQueue();
	}
	for (auto iter : m_vecPCMBufferPtr)
	{
		iter->clearBuffer();
	}

	//准备移动操作，计算要移动的位置
	auto midva = av_q2d(fileFormat->streams[m_iVideoStreamIndex]->time_base);
	long long videoPos = m_dSeekTime / midva;

	int ret = av_seek_frame(fileFormat, m_iVideoStreamIndex, videoPos, AVSEEK_FLAG_BACKWARD);
	if (0 != ret)
	{
		LOG_ERROR("seek video error:{}", ret);
	}
	avcodec_flush_buffers(videoCodecContext);
	avcodec_flush_buffers(audioCodecContext);

	//移动之后看一下实际上移动到了那个位置，然后再seek一下
	AVPacket* packet = av_packet_alloc(); // 分配一个数据包

	while (true)
	{
		if (av_read_frame(fileFormat, packet) >= 0)
		{
			if (packet->stream_index == m_iAudioStreamIndex)
			{
				continue;
			}
			else if (packet->stream_index == m_iVideoStreamIndex)
			{													 // 视频包需要解码
				//获取这一帧的时间戳，
				double pts = packet->pts * av_q2d(fileFormat->streams[m_iVideoStreamIndex]->time_base);
				m_dSeekTime = pts;
				//根据此时间戳，seek到这个时间戳
				auto midva = av_q2d(fileFormat->streams[m_iVideoStreamIndex]->time_base);
				auto videoPos = pts / midva;
				//根据实际的位置再seek一下
				av_seek_frame(fileFormat, m_iVideoStreamIndex, videoPos, AVSEEK_FLAG_BACKWARD);
				LOG_INFO("Try Seek Time:{},Really Seek Time Is:{}", m_dSeekTime.load(), videoPos);
				avcodec_flush_buffers(videoCodecContext);
				avcodec_flush_buffers(audioCodecContext);
				break;
			}
		}
	}
	
	m_ptrVideoReader->resume();
	m_ptrQueNeedDecodedPacket->resume();
	for (auto iter : m_vecQueDecodedPacket)
	{
		iter->resume();
	}
	m_bSeekState = false;
	m_bPauseState = false;
	m_PauseCV.notify_one();
}

void HardDecoder::decode()
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
		if (m_ptrVideoReader->getFinishedState() && 0 == m_ptrQueNeedDecodedPacket->getSize())
		{
			if (!m_bDecoderedFinished)
			{
				flushDecoder();
				m_bDecoderedFinished = true;
				if (m_finishedCallback)
				{
					m_finishedCallback();
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}
		std::shared_ptr<PacketWaitDecoded> packet = nullptr;
		if (m_ptrQueNeedDecodedPacket)
		{
			m_ptrQueNeedDecodedPacket->getPacket(packet);
		}
		if (packet)
		{
			//LOG_INFO("Get One Packet");
			switch (packet->type)
			{
			case PacketType::Video:
			{
				auto start = std::chrono::steady_clock::now();
				decodeVideo(packet);
				auto end = std::chrono::steady_clock::now();
				//printf("decoder time:%lld\n", std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
				LOG_INFO("Decoder Video Time:{}", std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
				break;
			}
			case PacketType::Audio:
			{
				decodeAudio(packet);
				break;
			}
			default:
				break;
			}
		}
		LOG_INFO("Decoder End");
	}
}

void HardDecoder::decodeVideo(std::shared_ptr<PacketWaitDecoded> packet)
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
			LOG_ERROR("Avcodec_receive_frame Error,Ret:{}", ret);
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
			for (auto& it : m_vecQueDecodedPacket)
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

void HardDecoder::decodeAudio(std::shared_ptr<PacketWaitDecoded> packet)
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
	if (avcodec_send_packet(audioCodecContext, packet->packet) == 0)
	{
		while (avcodec_receive_frame(audioCodecContext, frame) == 0)
		{
			LOG_INFO("Audio Decoder Begin Handle");
			//std::fstream fs("audio.pcm", std::ios::app | std::ios::binary);
			////把重采样之前的数据保存本地
			//fs.write((const char *)frame->data[0], frame->linesize[0]);
			//fs.close();

			int resampled_samples = av_rescale_rnd(
				swr_get_delay(swrContext, audioCodecContext->sample_rate) + frame->nb_samples,
				m_stuAudioInfo.audioSampleRate, audioCodecContext->sample_rate, AV_ROUND_UP);

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

			int converted_samples = swr_convert(swrContext, resampled_data, resampled_samples,
				(const uint8_t**)frame->data, frame->nb_samples);

			int pcmNumber = converted_samples * kOutputAudioChannels * av_get_bytes_per_sample(m_stuAudioInfo.audioFormat);

			//std::fstream fs("audio0.pcm", std::ios::app | std::ios::binary);
			////把重采样之后的数据保存本地
			//fs.write((const char*)resampled_data[0], pcmNumber);
			//fs.close();

			if (resampled_data)
			{
				for (auto& it : m_vecPCMBufferPtr)
				{
					it->appendData(resampled_data[0], pcmNumber);
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
