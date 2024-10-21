#include "AtomDecoder.h"

AtomDecoder::AtomDecoder()
	: formatContext(nullptr), videoCodecContext(nullptr),
	videoStreamIndex(-1), swsContext(nullptr)
{
}

AtomDecoder::~AtomDecoder()
{
	unInitModule();
}

int32_t AtomDecoder::initModule(const char* videoFileName, std::vector<std::pair<std::string, AudioInfo>> vecAudioFileNameAndInfo, const VideoInfo& outVideoInfo)
{
	if (m_bRunningState)
	{
		return -1;
	}
	m_vecAudioInfo = vecAudioFileNameAndInfo;
	m_stuVideoInfo = outVideoInfo;

	if (avformat_open_input(&formatContext, videoFileName, nullptr, nullptr) != 0)
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
			avcodec_parameters_to_context(videoCodecContext, codecParameters);
			avcodec_open2(videoCodecContext, codec, nullptr);

			swsContext = sws_getContext(
				videoCodecContext->width, videoCodecContext->height, videoCodecContext->pix_fmt,
				m_stuVideoInfo.width, m_stuVideoInfo.height, m_stuVideoInfo.videoFormat,
				SWS_BILINEAR, nullptr, nullptr, nullptr);
		}
	}
	// ��ȡ֡��
	AVRational frameRate = formatContext->streams[videoStreamIndex]->avg_frame_rate;
	auto fps = av_q2d(frameRate);
	m_uiReadThreadSleepTime = (kmilliSecondsPerSecond / fps) - 30 > 0 ? (kmilliSecondsPerSecond / fps) - 30 : 0;
	m_bInitState = true;

	for (auto& item : vecAudioFileNameAndInfo)
	{
		AVFormatContext* tmpFormatContext = nullptr;
		if (avformat_open_input(&tmpFormatContext, item.first.c_str(), nullptr, nullptr) != 0)
		{
			return (int32_t)ErrorCode::OpenInputError;
		}

		if (avformat_find_stream_info(tmpFormatContext, nullptr) < 0)
		{
			avformat_close_input(&tmpFormatContext);
			return (int32_t)ErrorCode::FindStreamInfoError;
		}

		for (unsigned int i = 0; i < tmpFormatContext->nb_streams; ++i)
		{
			AVCodecParameters* codecParameters = tmpFormatContext->streams[i]->codecpar;
			const AVCodec* codec = avcodec_find_decoder(codecParameters->codec_id);
			if (!codec)
			{
				continue;
			}
			if (codecParameters->codec_type == AVMEDIA_TYPE_AUDIO)
			{
				AVCodecContext* tmpCodecContext = nullptr;
				tmpCodecContext = avcodec_alloc_context3(codec);
				avcodec_parameters_to_context(tmpCodecContext, codecParameters);
				avcodec_open2(tmpCodecContext, codec, nullptr);

				SwrContext* tmpContext = swr_alloc();
				if (!tmpContext)
				{
					return (int32_t)ErrorCode::AllocateRsampleError;
				}

				AVChannelLayout in_channel_layout;
				av_channel_layout_default(&in_channel_layout, tmpCodecContext->ch_layout.nb_channels);

				AVChannelLayout out_channel_layout;
				av_channel_layout_default(&out_channel_layout, item.second.audioChannels); // Stereo output

				int out_sample_rate = item.second.audioSampleRate;
				// int out_sample_rate = audioCodecContext->sample_rate;
				AVSampleFormat out_sample_fmt = item.second.audioFormat;

				if (swr_alloc_set_opts2(&tmpContext, &out_channel_layout, out_sample_fmt, out_sample_rate,
					&in_channel_layout, tmpCodecContext->sample_fmt, tmpCodecContext->sample_rate, 0, nullptr) < 0)
				{
					swr_free(&tmpContext);
					return (int32_t)ErrorCode::AllocateRsampleError;
				}

				if (swr_init(tmpContext) < 0)
				{
					swr_free(&tmpContext);
					return (int32_t)ErrorCode::AllocateRsampleError;
				}
				Buffer* tmpBuffer = new Buffer();
				tmpBuffer->initBuffer(1024 * 10);
				m_vecBuffer.push_back(tmpBuffer);
				m_vecSwrContext.push_back(tmpContext);
				m_vecAudioFormatContext.push_back(tmpFormatContext);
				m_vecAudioCodecContext.push_back(std::make_pair(tmpCodecContext, i));
				break;
			}
		}
	}

	return (int32_t)ErrorCode::NoError;
}

void AtomDecoder::unInitModule()
{
	m_bRunningState = false;
	m_ReadVideoCV.notify_one();
	m_ReadAudioCV.notify_one();
	m_VideoCV.notify_one();
	m_AudioCV.notify_one();
	if (m_AudioDecoderThread.joinable())
	{
		m_AudioDecoderThread.join();
	}
	if (m_VideoDecoderThread.joinable())
	{
		m_VideoDecoderThread.join();
	}
	if (m_AudioDecoderThread.joinable())
	{
		m_AudioDecoderThread.join();
	}
	if (m_ReadVideoThread.joinable())
	{
		m_ReadVideoThread.join();
	}
	if (m_ReadAudioThread.joinable())
	{
		m_ReadAudioThread.join();
	}
	videoStreamIndex = -1;
	sws_freeContext(swsContext);
	for (auto& item : m_vecSwrContext)
	{
		swr_free(&item);
	}
	avcodec_free_context(&videoCodecContext);
	for (std::pair<AVCodecContext*, int32_t> item : m_vecAudioCodecContext)
	{
		avcodec_free_context(&item.first);
	}
	for (auto& item : m_vecAudioFormatContext)
	{
		avformat_close_input(&item);
	}
	avformat_close_input(&formatContext);
	for (auto& item : m_vecBuffer)
	{
		delete item;
		item = nullptr;
	}
}

void AtomDecoder::readFrameFromFile()
{
	if (!m_bInitState)
	{
		return;
	}

	while (m_bRunningState)
	{
		AVPacket* packet = av_packet_alloc(); // ����һ�����ݰ�
		{
			std::unique_lock<std::mutex> lck(m_PacketMutex);

			if (m_queueVideoFrame.size() > kBufferWaterLevel)
			{
				m_VideoCV.notify_one();
				m_ReadVideoCV.wait(lck, [this]
					{ return !m_bRunningState || m_queueVideoFrame.size() < kBufferWaterLevel; });
			}
		}
		if (!m_bRunningState)
		{
			break;
		}
		if (av_read_frame(formatContext, packet) >= 0)
		{
			if (packet->stream_index == videoStreamIndex)
			{                                                    // ��Ƶ����Ҫ����
				std::unique_lock<std::mutex> lck(m_PacketMutex); // ����Ƶ����������
				// qDebug() << "vi pts" << av_q2d(formatContext->streams[videoStreamIndex]->time_base) * packet->pts;
				if (av_q2d(formatContext->streams[videoStreamIndex]->time_base) * packet->pts < m_dSeekTime)
				{
					LOG_INFO("video frame late,pts:{}", av_q2d(formatContext->streams[videoStreamIndex]->time_base) * packet->pts);
				}
				m_queueVideoFrame.push(*packet); // ����Ƶ���������

				lck.unlock();
				m_VideoCV.notify_one(); // ����Ƶ����������
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(m_uiReadThreadSleepTime));
		}
		else
		{
			// ���ڶ�ȡ���ļ�ĩβ���˳�
			//  break;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			av_packet_unref(packet);
			// todo�������Ҫѭ�����ţ�����������seek���ļ���ͷ
		}
		
	}
}

void AtomDecoder::readAudioPacketFromFile()
{
	if (!m_bInitState)
	{
		return;
	}

	while (m_bRunningState)
	{
		for (int i = 0; i < m_vecAudioFormatContext.size(); ++i)
		{
			AVPacket* audioPacket = av_packet_alloc();
			{
				std::unique_lock<std::mutex> lck(m_AudioPacketMutex);

				if (m_queueAudioFrame.size() > kBufferWaterLevel)
				{
					m_AudioCV.notify_one();
					m_ReadAudioCV.wait(lck, [this]
						{ return !m_bRunningState || m_queueAudioFrame.size() < kBufferWaterLevel; });
				}
			}
			if (!m_bRunningState)
			{
				break;
			}
			if (av_read_frame(m_vecAudioFormatContext[i], audioPacket) >= 0)
			{
				if (audioPacket->stream_index == m_vecAudioCodecContext[i].second)
				{
					//double relative_pts = audioPacket->pts * av_q2d(m_vecAudioFormatContext[i]->streams[audioPacket->stream_index]->time_base);
					//qDebug() << "Relative PTS (seconds):" << relative_pts;
					// ��Ƶ����Ҫ����
					std::unique_lock<std::mutex> lck(m_AudioPacketMutex); // ����Ƶ����������
					// qDebug() << "ai pts" << av_q2d(formatContext->streams[audioStreamIndex]->time_base) * packet->pts;
					m_queueAudioFrame.push(std::pair<AVPacket, uint32_t>(*audioPacket, i)); // ����Ƶ���������
					lck.unlock();
					m_AudioCV.notify_one();
				}
			}
			else
			{
				// ���ڶ�ȡ���ļ�ĩβ���˳�
				//  break;
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				av_packet_unref(audioPacket);
				// todo�������Ҫѭ�����ţ�����������seek���ļ���ͷ
			}
		}
	}
}

void AtomDecoder::decodeVideo()
{
	AVFrame* frame = av_frame_alloc();
	while (m_bRunningState)
	{
		if (!m_bRunningState)
		{
			while (m_queueVideoFrame.size() > 0)
			{
				m_queueVideoFrame.pop();
			}
			break;
		}

		std::unique_lock<std::mutex> lck(m_PacketMutex);
		m_VideoCV.wait(lck, [this]
			{ return !m_bRunningState || !m_queueVideoFrame.empty(); });
		if (m_queueVideoFrame.size() <= 0)
		{
			lck.unlock();
			m_ReadVideoCV.notify_one();
			continue;
		}
		auto packet = m_queueVideoFrame.front();
		m_queueVideoFrame.pop();
		size_t size = m_queueVideoFrame.size();
		lck.unlock();
		if (size < kBufferWaterLevel)
		{
			m_ReadVideoCV.notify_one();
		}
		if (avcodec_send_packet(videoCodecContext, &packet) == 0)
		{
			int ret = avcodec_receive_frame(videoCodecContext, frame);
			// qDebug()<<"ret"<<ret;
			while (ret == 0)
			{
				AVFrame* yuvFrame = av_frame_alloc();
				av_image_alloc(yuvFrame->data, yuvFrame->linesize, m_stuVideoInfo.width, m_stuVideoInfo.height, m_stuVideoInfo.videoFormat, 1);
				sws_scale(swsContext, frame->data, frame->linesize, 0, videoCodecContext->height, yuvFrame->data, yuvFrame->linesize);
				VideoCallbackInfo videoInfo;
				videoInfo.width = m_stuVideoInfo.width;
				videoInfo.height = m_stuVideoInfo.height;
				videoInfo.videoFormat = m_stuVideoInfo.videoFormat;
				// ����avframe�е�������
				switch (m_stuVideoInfo.videoFormat)
				{
				case AV_PIX_FMT_YUV420P:
				{
					videoInfo.yuvData = new uint8_t[m_stuVideoInfo.width * m_stuVideoInfo.height * 3 / 2];
					videoInfo.dataSize = m_stuVideoInfo.width * m_stuVideoInfo.height * 3 / 2;
					memcpy(videoInfo.yuvData, yuvFrame->data[0], videoInfo.dataSize);
				}
				break;
				case AV_PIX_FMT_YUV422P:
				case AV_PIX_FMT_YUYV422:
				case AV_PIX_FMT_UYVY422:
				{
					videoInfo.yuvData = new uint8_t[m_stuVideoInfo.width * m_stuVideoInfo.height * 2];
					videoInfo.dataSize = m_stuVideoInfo.width * m_stuVideoInfo.height * 2;
					memcpy(videoInfo.yuvData, yuvFrame->data[0], videoInfo.dataSize);
				}
				break;
				default:
					break;
				}
				av_freep(yuvFrame->data);
				av_frame_free(&yuvFrame);
				if (nullptr == videoInfo.yuvData)
				{
					LOG_ERROR("videoInfo.yuvData is nullptr");
					ret = avcodec_receive_frame(videoCodecContext, frame);
					continue;
				}
				double pts = frame->pts * av_q2d(formatContext->streams[videoStreamIndex]->time_base);
				if (pts > 0)
				{
					m_uiVideoCurrentTime = pts * kmicroSecondsPerSecond;
				}

				// ��ʾ�Ӳ��ſ�ʼ����ǰʱ����������ʱ�䣨��΢��Ϊ��λ����ͨ����ȥ m_iStartTime�����ǵõ��Ӳ��ſ�ʼ�����ڵ�ʵ�ʲ���ʱ��
				uint64_t current_time = av_gettime() - m_iStartTime;
				// ��ʾ��ǰ֡��Ҫ�ӳ���ʾ��ʱ�䡣ͨ������ pts Ӧ����ʾ��ʱ���� current_time �Ĳ�ֵ�����ǵõ���Ҫ�ȴ���ʱ�䣬��ȷ��֡����ȷ��ʱ����ʾ
				int64_t delay = static_cast<int64_t>(pts * kmicroSecondsPerSecond) - current_time;

				// qDebug() << "video start time" << m_iStartTime << ",play time" << current_time << "seek time" << m_iTotalVideoSeekTime << ",delay:" << delay << ",pts:" << pts;

				if (delay > 0)
				{
					std::this_thread::sleep_for(std::chrono::microseconds(delay));
				}

				if (m_previewCallback)
				{
					m_previewCallback(videoInfo, pts);
					if (m_AudioCallback)
					{
						m_AudioCallback(m_vecBuffer);
					}
				}
				ret = avcodec_receive_frame(videoCodecContext, frame);
			}
		}
		else
		{
			LOG_ERROR("video avcodec_send_packet error");
		}
		av_packet_unref(&packet);
	}
	av_frame_free(&frame);
}

void AtomDecoder::decodeAudio()
{
	AVFrame* frame = av_frame_alloc();
	int swr_size = 0;
	std::fstream fs("audio.pcm", std::ios::out | std::ios::binary);
	while (m_bRunningState)
	{
		if (!m_bRunningState)
		{
			while (m_queueAudioFrame.size() > 0)
			{
				m_queueAudioFrame.pop();
			}
			break;
		}
		std::unique_lock<std::mutex> lck(m_AudioPacketMutex);
		m_AudioCV.wait(lck, [this]()
			{ return !m_bRunningState || !m_queueAudioFrame.empty(); });
		if (m_queueAudioFrame.size() <= 0)
		{
			lck.unlock();
			m_ReadAudioCV.notify_one();
			continue;
		}
		auto audioInfo = m_queueAudioFrame.front();
		auto packet = audioInfo.first;
		uint32_t index = audioInfo.second;
		m_queueAudioFrame.pop();
		size_t size = m_queueAudioFrame.size();
		lck.unlock();
		if (size < kBufferWaterLevel)
		{
			m_ReadAudioCV.notify_one();
		}
		if (avcodec_send_packet(m_vecAudioCodecContext[index].first, &packet) == 0)
		{
			while (avcodec_receive_frame(m_vecAudioCodecContext[index].first, frame) == 0)
			{
				int data_size = av_samples_get_buffer_size(nullptr,
					m_vecAudioCodecContext[index].first->ch_layout.nb_channels,
					frame->nb_samples,
					m_vecAudioCodecContext[index].first->sample_fmt, 1);
				int32_t out_buffer_size = av_samples_get_buffer_size(nullptr, m_vecAudioInfo[index].second.audioChannels, frame->nb_samples, m_vecAudioInfo[index].second.audioFormat, 1);

				//// ��������������Ŀռ�
				uint8_t* out_buff = (unsigned char*)av_malloc(out_buffer_size);
				swr_size = swr_convert(m_vecSwrContext[index],										  // ��Ƶ��������ʵ��
					&out_buff, frame->nb_samples,					  // ������������ݺ����ݴ�С
					(const uint8_t**)frame->data, frame->nb_samples); // ������������ݺ����ݴ�С

				double pts = frame->pts * av_q2d(m_vecAudioFormatContext[index]->streams[m_vecAudioCodecContext[index].second]->time_base);
				// ��¼�µ�ǰ���ŵ�֡��ʱ�䣬���ڼ�����ʱ������
				if (pts > 0)
				{
					m_uiAudioCurrentTime = pts * kmicroSecondsPerSecond;
				}

				int64_t current_time = av_gettime() - m_iStartTime;
				int64_t delay = static_cast<int64_t>(pts * kmicroSecondsPerSecond) - current_time;

				//qDebug() << "audio start time" << m_iStartTime << ",play time" << current_time << "seek time" << m_iTotalAudioSeekTime << ",delay:" << delay << ",pts:" << pts;

				if (delay > 0)
				{
					std::this_thread::sleep_for(std::chrono::microseconds(delay));
				}

				if (0 == index)
				{
					QByteArray data((char*)out_buff, frame->nb_samples * av_get_bytes_per_sample(m_vecAudioInfo[index].second.audioFormat));
					fs.write(data.data(), data.size());
				}

				m_vecBuffer[index]->appendData(out_buff, frame->nb_samples * av_get_bytes_per_sample(m_vecAudioInfo[index].second.audioFormat));

				if (out_buff)
				{
					av_freep(&out_buff);
				}
			}
		}

		av_packet_unref(&packet);
	}
	fs.close();
	av_frame_free(&frame);
}

void AtomDecoder::initVideoCallBack(PreviewCallback preCallback, VideoOutputCallback videoOutputCallback)
{
	m_previewCallback = preCallback;
}

void AtomDecoder::initAudioCallback(AudioCallback audioCallback)
{
	m_AudioCallback = audioCallback;
}

void AtomDecoder::startDecoder()
{
	if (!m_bInitState)
	{
		return;
	}
	m_bRunningState = true;

	m_iStartTime = av_gettime();

	m_ReadVideoThread = std::thread(&AtomDecoder::readFrameFromFile, this);

	m_ReadAudioThread = std::thread(&AtomDecoder::readAudioPacketFromFile, this);

	m_VideoDecoderThread = std::thread(&AtomDecoder::decodeVideo, this);

	m_AudioDecoderThread = std::thread(&AtomDecoder::decodeAudio, this);
}