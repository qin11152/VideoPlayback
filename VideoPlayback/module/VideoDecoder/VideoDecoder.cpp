#include "VideoDecoder.h"

#include <fstream>
#include <thread>

#define MAX_AUDIO_FRAME_SIZE 80960

static double r2d(AVRational r)
{
	return r.den == 0 ? 0 : (double)r.num / (double)r.den;
}

std::function<void(AVFrame *)> avframedel = [](AVFrame *_frame)
{
	av_freep(_frame->data);
	av_frame_free(&_frame); /*fprintf(stdout, "AVFrame clear\n");*/
};

VideoDecoder::VideoDecoder()
	: formatContext(nullptr), videoCodecContext(nullptr), audioCodecContext(nullptr),
	  videoStreamIndex(-1), audioStreamIndex(-1), swsContext(nullptr), swrContext(nullptr)
{
}

VideoDecoder::~VideoDecoder()
{
	unInitModule();
}

int32_t VideoDecoder::initModule(const char *fileName, const VideoInfo &outVideoInfo, const AudioInfo &outAudioInfo)
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
		AVCodecParameters *codecParameters = formatContext->streams[i]->codecpar;
		const AVCodec *codec = avcodec_find_decoder(codecParameters->codec_id);
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
		else if (codecParameters->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIndex == -1)
		{
			audioStreamIndex = i;

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
			//int out_sample_rate = audioCodecContext->sample_rate;
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
	m_uiReadThreadSleepTime = (kmilliSecondsPerSecond / fps) - 30 > 0 ? (kmilliSecondsPerSecond / fps) - 30 : 0;
	m_bInitState = true;
	return (int32_t)ErrorCode::NoError;
}

void VideoDecoder::unInitModule()
{
	m_bRunningState = false;
	if (m_bPauseState)
	{
		m_PauseCV.notify_all();
	}
	m_ReadCV.notify_one();
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
	if (m_ReadThread.joinable())
	{
		m_ReadThread.join();
	}
	audioStreamIndex = -1;
	videoStreamIndex = -1;
	sws_freeContext(swsContext);
	swr_free(&swrContext);
	avcodec_free_context(&videoCodecContext);
	avcodec_free_context(&audioCodecContext);
	avformat_close_input(&formatContext);
}

void VideoDecoder::readFrameFromFile()
{
	if (!m_bInitState)
	{
		return;
	}

	while (m_bRunningState)
	{
		AVPacket *packet = av_packet_alloc(); // 分配一个数据包
		{
			std::unique_lock<std::mutex> lck(m_PacketMutex);

			if (m_bSeekState)
			{
				m_bSeekState = false;
				m_bFirstVideoPacketAfterSeek = true;
				m_bFirstAudioPacketAfterSeek = true;
				m_bFirstReadedVideoPakcet = true;

				auto midva = (double)r2d(formatContext->streams[videoStreamIndex]->time_base);
				long long videoPos = m_dSeekTime / midva;

				midva = (double)r2d(formatContext->streams[audioStreamIndex]->time_base);
				long long audioPos = m_dSeekTime / midva;

				int ret = av_seek_frame(formatContext, videoStreamIndex, videoPos, AVSEEK_FLAG_BACKWARD);
				// ret = av_seek_frame(formatContext, audioStreamIndex, audioPos, AVSEEK_FLAG_BACKWARD);

				m_iTotalVideoSeekTime += m_dSeekTime * kmicroSecondsPerSecond - m_uiVideoCurrentTime;
				LOG_INFO("at seek time,play time:{},seek time{},video changed time:{}", (double)m_uiVideoCurrentTime / kmicroSecondsPerSecond, m_dSeekTime.load(), m_iTotalVideoSeekTime);
				m_uiVideoCurrentTime = m_dSeekTime * kmicroSecondsPerSecond;
				m_iTotalAudioSeekTime += m_dSeekTime * kmicroSecondsPerSecond - m_uiAudioCurrentTime;
				LOG_INFO("at seek time,play time:{},seek time{:.4f},audio changed time:{}", (double)m_uiAudioCurrentTime / kmicroSecondsPerSecond, m_dSeekTime.load(), m_iTotalAudioSeekTime);
				m_uiAudioCurrentTime = m_dSeekTime * kmicroSecondsPerSecond;

				avcodec_flush_buffers(videoCodecContext);
				// avcodec_flush_buffers(audioCodecContext);

				while (m_queueVideoFrame.size() > 0)
				{
					m_queueVideoFrame.pop();
				}
				while (m_queueAudioFrame.size() > 0)
				{
					m_queueAudioFrame.pop();
				}
			}

			if (m_queueAudioFrame.size() > kBufferWaterLevel && m_queueVideoFrame.size() > kBufferWaterLevel)
			{
				m_VideoCV.notify_one();
				m_AudioCV.notify_one();
				m_ReadCV.wait(lck, [this]
							  { return !m_bRunningState || m_queueAudioFrame.size() < kBufferWaterLevel || m_queueVideoFrame.size() < kBufferWaterLevel || m_bSeekState; });
			}
			if (m_bSeekState)
			{
				continue;
			}
		}
		if (!m_bRunningState)
		{
			break;
		}
		if (av_read_frame(formatContext, packet) >= 0)
		{
			if (packet->stream_index == audioStreamIndex)
			{
				// 音频包需要解码
				std::unique_lock<std::mutex> lck(m_PacketMutex); // 对音频队列锁加锁
				// qDebug() << "ai pts" << av_q2d(formatContext->streams[audioStreamIndex]->time_base) * packet->pts;
				if (av_q2d(formatContext->streams[audioStreamIndex]->time_base) * packet->pts < m_dSeekTime)
				{
					LOG_INFO("audio frame late,pts:{}", av_q2d(formatContext->streams[audioStreamIndex]->time_base) * packet->pts);
				}
				m_queueAudioFrame.push(*packet); // 把音频包加入队列
				lck.unlock();
				m_AudioCV.notify_one();
			}
			else if (packet->stream_index == videoStreamIndex)
			{													 // 视频包需要解码
				std::unique_lock<std::mutex> lck(m_PacketMutex); // 对视频队列锁加锁
				// qDebug() << "vi pts" << av_q2d(formatContext->streams[videoStreamIndex]->time_base) * packet->pts;
				if (av_q2d(formatContext->streams[videoStreamIndex]->time_base) * packet->pts < m_dSeekTime)
				{
					LOG_INFO("video frame late,pts:{}", av_q2d(formatContext->streams[videoStreamIndex]->time_base) * packet->pts);
				}
				m_queueVideoFrame.push(*packet); // 把视频包加入队列

				lck.unlock();
				m_VideoCV.notify_one(); // 对视频队列锁解锁
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(m_uiReadThreadSleepTime));
		}
		else
		{
			// 现在读取到文件末尾就退出
			//  break;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			av_packet_unref(packet);
			// todo，如果需要循环播放，可以在这里seek到文件开头
		}
	}
}

void VideoDecoder::decodeVideo()
{
	AVFrame *frame = av_frame_alloc();
	while (m_bRunningState)
	{
		if (m_bPauseState)
		{
			std::unique_lock<std::mutex> lckp(m_PauseMutex);
			m_PauseCV.wait(lckp, [this]()
						   { return !m_bPauseState || !m_bRunningState; });
		}

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
		if (m_bSeekState)
		{
			lck.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			continue;
		}
		if (m_queueVideoFrame.size() <= 0)
		{
			lck.unlock();
			continue;
		}
		auto packet = m_queueVideoFrame.front();
		m_queueVideoFrame.pop();
		size_t size = m_queueVideoFrame.size();
		lck.unlock();
		if (size < kBufferWaterLevel)
		{
			m_ReadCV.notify_one();
		}
		if (avcodec_send_packet(videoCodecContext, &packet) == 0)
		{
			int ret = avcodec_receive_frame(videoCodecContext, frame);
			// qDebug()<<"ret"<<ret;
			while (ret == 0)
			{
				AVFrame *yuvFrame = av_frame_alloc();
				av_image_alloc(yuvFrame->data, yuvFrame->linesize, m_stuVideoInfo.width, m_stuVideoInfo.height, m_stuVideoInfo.videoFormat, 1);
				sws_scale(swsContext, frame->data, frame->linesize, 0, videoCodecContext->height, yuvFrame->data, yuvFrame->linesize);
				VideoCallbackInfo videoInfo;
				videoInfo.width = m_stuVideoInfo.width;
				videoInfo.height = m_stuVideoInfo.height;
				videoInfo.videoFormat = m_stuVideoInfo.videoFormat;
				// 计算avframe中的数据量
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
					// qDebug() << "video pts" << pts;
					if (m_bFirstVideoPacketAfterSeek)
					{
						m_bFirstVideoPacketAfterSeek = false;
					}
					m_uiVideoCurrentTime = pts * kmicroSecondsPerSecond;
				}

				// 表示从播放开始到当前时刻所经过的时间（以微秒为单位）。通过减去 m_iStartTime，我们得到从播放开始到现在的实际播放时间
				uint64_t current_time = av_gettime() - m_iStartTime + m_iTotalVideoSeekTime;
				// 表示当前帧需要延迟显示的时间。通过计算 pts 应该显示的时间与 current_time 的差值，我们得到需要等待的时间，以确保帧在正确的时间显示
				int64_t delay = static_cast<int64_t>(pts * kmicroSecondsPerSecond) - current_time;

				if (delay > 0)
				{
					std::this_thread::sleep_for(std::chrono::microseconds(delay));
				}

				if (m_previewCallback && !m_bSeekState)
				{
					m_previewCallback(videoInfo, pts);
				}
				if (m_videoOutputCallback && !m_bSeekState)
				{
					m_videoOutputCallback(videoInfo);
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

void VideoDecoder::decodeAudio()
{
	AVFrame *frame = av_frame_alloc();
	int swr_size = 0;
	while (m_bRunningState)
	{
		if (m_bPauseState)
		{
			std::unique_lock<std::mutex> lckp(m_PauseMutex);
			m_PauseCV.wait(lckp, [this]()
						   { return !m_bPauseState || !m_bRunningState; });
		}
		if (!m_bRunningState)
		{
			while (m_queueAudioFrame.size() > 0)
			{
				m_queueAudioFrame.pop();
			}
			break;
		}
		std::unique_lock<std::mutex> lck(m_PacketMutex);
		m_AudioCV.wait(lck, [this]()
					   { return !m_bRunningState || !m_queueAudioFrame.empty(); });
		if (m_bSeekState)
		{
			lck.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			continue;
		}
		if (m_queueAudioFrame.size() <= 0)
		{
			lck.unlock();
			continue;
		}
		auto packet = m_queueAudioFrame.front();
		m_queueAudioFrame.pop();
		size_t size = m_queueAudioFrame.size();
		lck.unlock();
		if (size < kBufferWaterLevel)
		{
			m_ReadCV.notify_one();
		}

		if (avcodec_send_packet(audioCodecContext, &packet) == 0)
		{
			while (avcodec_receive_frame(audioCodecContext, frame) == 0)
			{
				int data_size = av_samples_get_buffer_size(nullptr,
														   audioCodecContext->ch_layout.nb_channels,
														   frame->nb_samples,
														   audioCodecContext->sample_fmt, 1);
				int32_t out_buffer_size = av_samples_get_buffer_size(nullptr, m_stuAudioInfo.audioChannels, frame->nb_samples, m_stuAudioInfo.audioFormat, 1);

				//// 分配输出缓冲区的空间
				uint8_t* out_buff = (unsigned char*)av_malloc(out_buffer_size);
				swr_size = swr_convert(swrContext,										  // 音频采样器的实例
									   &out_buff, frame->nb_samples,						  // 输出的数据内容和数据大小
									   (const uint8_t **)frame->data, frame->nb_samples); // 输入的数据内容和数据大小

				double pts = frame->pts * av_q2d(formatContext->streams[audioStreamIndex]->time_base);
				// 记录下当前播放的帧的时间，用于计算快进时的增量

				if (pts > 0)
				{
					// qDebug() << "audio pts" << pts;
					if (m_bFirstAudioPacketAfterSeek)
					{
						m_bFirstAudioPacketAfterSeek = false;
					}
					m_uiAudioCurrentTime = pts * kmicroSecondsPerSecond;
				}

				int64_t current_time = av_gettime() - m_iStartTime + m_iTotalAudioSeekTime;
				int64_t delay = static_cast<int64_t>(pts * kmicroSecondsPerSecond) - current_time;

				// qDebug() << "start time" << m_iStartTime << ",play time" << current_time << "seek time" << m_iTotalAudioSeekTime << ",delay:" << delay << ",pts:" << pts;

				if (delay > 0)
				{
					std::this_thread::sleep_for(std::chrono::microseconds(delay));
				}

				if (m_audioPlayCallback && !m_bSeekState)
				{
					m_audioPlayCallback(out_buff, swr_size);
				}
				// file.write((const char*)out_buff, out_buffer_size);
				if (out_buff)
				{
					av_freep(&out_buff);
				}
			}
		}

		av_packet_unref(&packet);
	}
	av_frame_free(&frame);
}

void VideoDecoder::initVideoCallBack(PreviewCallback preCallback, VideoOutputCallback videoOutputCallback)
{
	m_previewCallback = preCallback;
	m_videoOutputCallback = videoOutputCallback;
}

void VideoDecoder::pauseDecoder()
{
	std::unique_lock<std::mutex> lck(m_PauseMutex);
	m_iPauseTime = av_gettime();
	m_bPauseState = true;
}

void VideoDecoder::resumeDecoder()
{
	std::unique_lock<std::mutex> lck(m_PauseMutex);
	m_bPauseState = false;
	int pauseDuration = av_gettime() - m_iPauseTime;
	m_iStartTime += pauseDuration;
	m_PauseCV.notify_all();
}

void VideoDecoder::seekTo(double_t time)
{
	if (m_bSeekState)
	{
		return;
	}
	if (!m_bInitState)
	{
		return;
	}
	if (time < 0 || time > formatContext->duration)
	{
		return;
	}
	m_dSeekTime = time;
	m_bSeekState = true;
	qDebug() << "seek value" << time;
	std::unique_lock<std::mutex> lck(m_PacketMutex);
	m_ReadCV.notify_one();
}

void VideoDecoder::clearBuffer()
{
}

void VideoDecoder::initAudioCallback(AudioPlayCallback audioCallback)
{
	m_audioPlayCallback = audioCallback;
}

void VideoDecoder::startDecoder()
{
	if (!m_bInitState)
	{
		return;
	}
	m_bRunningState = true;

	m_iStartTime = av_gettime();

	m_ReadThread = std::thread(&VideoDecoder::readFrameFromFile, this);

	m_VideoDecoderThread = std::thread(&VideoDecoder::decodeVideo, this);

	m_AudioDecoderThread = std::thread(&VideoDecoder::decodeAudio, this);
}