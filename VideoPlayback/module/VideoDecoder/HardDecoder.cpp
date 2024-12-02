#include "HardDecoder.h"

HardDecoder::HardDecoder() 
	: formatContext(nullptr), videoCodecContext(nullptr), audioCodecContext(nullptr),
videoStreamIndex(-1), audioStreamIndex(-1), swsContext(nullptr), swrContext(nullptr)
{

}

HardDecoder::~HardDecoder()
{
	unInitModule();
}

int32_t HardDecoder::initModule(const char* fileName, const VideoInfo& outVideoInfo, const AudioInfo& outAudioInfo, const enum AVHWDeviceType type)
{
	if (m_bRunningState)
	{
		return -1;
	}
	m_stuAudioInfo = outAudioInfo;
	m_stuVideoInfo = outVideoInfo;

	if (avformat_open_input(&formatContext, fileName, nullptr, nullptr) != 0)
	{
		return (int32_t)ErrorCode::OpenInputError;
	}

	if (avformat_find_stream_info(formatContext, nullptr) < 0)
	{
		avformat_close_input(&formatContext);
		return (int32_t)ErrorCode::FindStreamInfoError;
	}

	for (unsigned int i = 0; i < formatContext->nb_streams; ++i)
	{
		AVCodecParameters* codecParameters = formatContext->streams[i]->codecpar;
		const AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);
		if (!codec)
		{
			continue;
		}

		if (codecParameters->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIndex == -1)
		{
			videoStreamIndex = i;
			videoCodecContext = avcodec_alloc_context3(codec);
			if (!videoCodecContext) {
				return (int32_t)ErrorCode::AllocateContextError;
			}

			// 创建硬件设备上下文
			AVBufferRef* hwDeviceCtx = nullptr;
			int err = av_hwdevice_ctx_create(&hwDeviceCtx, type, nullptr, nullptr, 0);
			if (err < 0) 
			{
				avcodec_free_context(&videoCodecContext);
				return (int32_t)ErrorCode::AllocateContextError;
			}

			// 设置硬件设备上下文
			videoCodecContext->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
			av_buffer_unref(&hwDeviceCtx);

			if (avcodec_parameters_to_context(videoCodecContext, codecParameters) < 0) 
			{
				avcodec_free_context(&videoCodecContext);
				return (int32_t)ErrorCode::AllocateContextError;
			}

			// 打开解码器
			if (avcodec_open2(videoCodecContext, codec, nullptr) < 0) {
				avcodec_free_context(&videoCodecContext);
				return (int32_t)ErrorCode::AllocateContextError;
			}
		}
		else if (codecParameters->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIndex == -1)
		{
			audioStreamIndex = i;

			//解码器参数
			audioCodecContext = avcodec_alloc_context3(codec);
			avcodec_parameters_to_context(audioCodecContext, codecParameters);
			avcodec_open2(audioCodecContext, codec, nullptr);

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
		}
	}

	// 获取帧率
	AVRational frameRate = formatContext->streams[videoStreamIndex]->avg_frame_rate;
	auto fps = av_q2d(frameRate);
	m_uiReadThreadSleepTime = (kmilliSecondsPerSecond / fps);
	m_uiPerFrameSampleCnt = m_stuAudioInfo.audioSampleRate / fps;
	if (nullptr == m_ptrPCMBuffer)
	{
		m_ptrPCMBuffer = new Buffer();
		m_ptrPCMBuffer->initBuffer(1024 * 10);
	}

	m_bInitState = true;
	return (int32_t)ErrorCode::NoError;

}

void HardDecoder::unInitModule()
{
	m_bRunningState = false;
	if (m_bPauseState)
	{
		m_PauseCV.notify_all();
	}
	m_ReadCV.notify_all();
	m_queueWaitDecodedCV.notify_all();
	m_queueWaitConsumedCV.notify_all();
	if (m_ReadThread.joinable())
	{
		m_ReadThread.join();
	}
	if (m_DecoderThread.joinable())
	{
		m_DecoderThread.join();
	}

	audioStreamIndex = -1;
	videoStreamIndex = -1;
	if (m_ptrPCMBuffer)
	{
		delete m_ptrPCMBuffer;
		m_ptrPCMBuffer = nullptr;
	}
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
	if (formatContext)
	{
		avformat_close_input(&formatContext);
		formatContext = nullptr;
	}
}

void HardDecoder::startDecoder()
{
	if (!m_bInitState)
	{
		return;
	}
	m_bRunningState = true;

	m_ReadThread = std::thread(&HardDecoder::readFrameFromFile, this);

	m_DecoderThread = std::thread(&HardDecoder::decoder, this);
}

void HardDecoder::decoder()
{
	if (!m_bInitState)
	{
		return;
	}
	AVPacket* packet = nullptr;
	PacketType type;
	while (true)
	{
		{
			std::unique_lock<std::mutex> lck(m_afterDecoderInfoMutex);
			if (m_bSeekState)
			{
				m_SeekCV.wait(lck, [this]() {return !m_bSeekState || !m_bRunningState; });
			}
			//解码后的视频队列大于阈值，就等待消费线程消费，暂停解码
			if (m_queueVideoInfo.size() > kAfterDecoderCachedCnt)
			{
				//等待消费线程消费
				m_queueWaitConsumedCV.wait(lck, [this]
					{ return !m_bRunningState || m_queueVideoInfo.size() < kAfterDecoderCachedCnt; });
				LOG_INFO("After Consume Queue Not Full");
			}
		}
		{
			LOG_INFO("Decoder Wait Get Lock");
			std::unique_lock<std::mutex> lck(m_queueMutex);
			LOG_INFO("Decoder Get Lock");
			//待解码队列空了，就阻塞等待读取线程写入，同时notify
			if (m_queueNeedDecoderPacket.size() <= 0)
			{
				m_ReadCV.notify_one();
				m_queueWaitDecodedCV.wait(lck, [this]
					{ return !m_bRunningState || m_queueNeedDecoderPacket.size() > 0; });
				LOG_INFO("After Decoder Queue Empty");
			}
			if (m_queueNeedDecoderPacket.size() > 0)
			{
				packet = m_queueNeedDecoderPacket.front().first;
				type = m_queueNeedDecoderPacket.front().second;
				m_queueNeedDecoderPacket.pop();
				LOG_INFO("Decoder Release Lock");
				lck.unlock();
				//取了一个以后就通知读取线程继续读取
				m_ReadCV.notify_one();
			}
		}
		if (!m_bRunningState)
		{
			break;
		}

		if (packet)
		{
			LOG_INFO("Get One Packet");
			switch (type)
			{
			case PacketType::Video:
			{
				auto start = std::chrono::steady_clock::now();
				decoderVideo(packet);
				auto end = std::chrono::steady_clock::now();
				printf("decoder time:%lld\n", std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
				break;
			}
			case PacketType::Audio:
			{
				decoderAudio(packet);
				break;
			}
			default:
				break;
			}
		}
		LOG_INFO("Decoder End");
	}
	av_packet_unref(packet);
}

void HardDecoder::decoderVideo(AVPacket* packet)
{
	AVFrame* frame = av_frame_alloc();
	AVFrame* swFrame = av_frame_alloc(); // 用于硬件帧转换
	int ret = 0;

	if (avcodec_send_packet(videoCodecContext, packet) == 0)
	{
		auto decode_start = std::chrono::steady_clock::now();
		int ret = avcodec_receive_frame(videoCodecContext, frame);
		auto decode_end = std::chrono::steady_clock::now();

		while (ret == 0)
		{
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
					printf("decoder time:%lld,transfer time: %lld, convert time: %lld\n", std::chrono::duration_cast<std::chrono::milliseconds>(decode_end-decode_start).count(), std::chrono::duration_cast<std::chrono::milliseconds>(transfer_end - transfer_start).count(), std::chrono::duration_cast<std::chrono::milliseconds>(convert_end - convert_start).count());
					LOG_INFO("Video Decoder Convert");
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
				//switch (frame->format)
				//{
				//case AV_PIX_FMT_YUV420P:
				//{}
				//break;
				//default:
				//	break;
				//}
				auto convert_start = std::chrono::steady_clock::now();
				AVFrame* yuvFrame = av_frame_alloc();
				av_image_alloc(yuvFrame->data, yuvFrame->linesize, m_stuVideoInfo.width, m_stuVideoInfo.height, m_stuVideoInfo.videoFormat, 1);

				sws_scale(swsContext, frame->data, frame->linesize, 0, videoCodecContext->height, yuvFrame->data, yuvFrame->linesize);
				auto convert_end = std::chrono::steady_clock::now();
				printf("decoder time:%lld, convert time: %lld\n", std::chrono::duration_cast<std::chrono::milliseconds>(decode_end - decode_start).count(), std::chrono::duration_cast<std::chrono::milliseconds>(convert_end - convert_start).count());
				LOG_INFO("Video Decoder Convert");
				av_freep(yuvFrame->data);
				av_frame_free(&yuvFrame);
			}
			decode_start = std::chrono::steady_clock::now();
			ret = avcodec_receive_frame(videoCodecContext, frame);
			decode_end = std::chrono::steady_clock::now();
		}
		av_frame_free(&frame);
	}
}

void HardDecoder::decoderAudio(AVPacket* packet)
{

}

void HardDecoder::readFrameFromFile()
{
	if (!m_bInitState)
	{
		return;
	}

	while (m_bRunningState)
	{
		AVPacket* packet = av_packet_alloc(); // 分配一个数据包
		LOG_INFO("Read Wait Get Lock");
		std::unique_lock<std::mutex> lck(m_queueMutex);
		//首先查看等待解码的队列是否已经满了，如果满了就等待解码线程消费
		LOG_INFO("Read Get Lock");
		if (m_bSeekState)
		{
			m_SeekCV.wait(lck, [this]
				{ return !m_bRunningState || !m_bSeekState; });
		}
		if (m_queueNeedDecoderPacket.size() > kBufferWaterLevel)
		{
			m_ReadCV.wait(lck, [this]
				{ return !m_bRunningState || m_queueNeedDecoderPacket.size() < kBufferWaterLevel; });
			LOG_INFO("Wait packet queue not full");
		}
		lck.unlock();
		if (!m_bRunningState)
		{
			break;
		}
		if (av_read_frame(formatContext, packet) >= 0)
		{
			if (packet->stream_index == audioStreamIndex)
			{
				// 音频包需要解码
				LOG_INFO("Audio Wait Get Lock");
				std::unique_lock<std::mutex> lck(m_queueMutex); // 对音频队列锁加锁
				LOG_INFO("Audio Get Lock,Before Queue Size:{}", m_queueNeedDecoderPacket.size());
				m_queueNeedDecoderPacket.push(std::make_pair(packet, PacketType::Audio)); // 把音频包加入队列
				lck.unlock();
				LOG_INFO("Audio Release Lock");
				//读取了一个音频包就通知解码线程，可以解码了
				m_queueWaitDecodedCV.notify_one();
			}
			else if (packet->stream_index == videoStreamIndex)
			{
				// 视频包需要解码
				LOG_INFO("Video Wait Get Lock");
				std::unique_lock<std::mutex> lck(m_queueMutex); // 对视频队列锁加锁
				LOG_INFO("Video Get Lock,Before Queue Size:{}", m_queueNeedDecoderPacket.size());
				m_queueNeedDecoderPacket.push(std::make_pair(packet, PacketType::Video)); // 把视频包加入队列
				lck.unlock();
				LOG_INFO("Video Release Lock");
				//读取了一个视频包就通知解码线程，可以解码了
				m_queueWaitDecodedCV.notify_one();
			}
			//std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		else
		{
			// 现在读取到文件末尾就退出
			av_packet_unref(packet);
			break;
			// todo，如果需要循环播放，可以在这里seek到文件开头
		}
		LOG_INFO("Read End");
	}
}
