#define _CRT_SECURE_NO_WARNINGS

#include "VideoDecoder.h"

#include <fstream>
#include <thread>
#include <sstream>
#include <iomanip>

std::string getTime(const std::chrono::system_clock::time_point currentTime)
{
	auto in_time_t = std::chrono::system_clock::to_time_t(currentTime);
	std::tm tm = *std::localtime(&in_time_t);

	auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime.time_since_epoch()) % 1000;

	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
	oss << '.' << std::setw(3) << std::setfill('0') << milliseconds.count();
	return oss.str();
}

void preciseSleep(std::chrono::microseconds duration) 
{
	auto start = std::chrono::high_resolution_clock::now();
	while (std::chrono::high_resolution_clock::now() - start < duration) {
		// 忙等待
	}
}

#define MAX_AUDIO_FRAME_SIZE 80960

static double r2d(AVRational r)
{
	return r.den == 0 ? 0 : (double)r.num / (double)r.den;
}

std::function<void(AVFrame*)> avframedel = [](AVFrame* _frame)
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

int32_t VideoDecoder::initModule(const char* fileName, const VideoInfo& outVideoInfo, const AudioInfo& outAudioInfo)
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
			avcodec_parameters_to_context(videoCodecContext, codecParameters);
			avcodec_open2(videoCodecContext, codec, nullptr);

			swsContext = sws_getContext(
				videoCodecContext->width, videoCodecContext->height, videoCodecContext->pix_fmt,
				videoCodecContext->width, videoCodecContext->height, m_stuVideoInfo.videoFormat,
				SWS_BILINEAR, nullptr, nullptr, nullptr);
			m_stuVideoInfo.width = videoCodecContext->width;
			m_stuVideoInfo.height = videoCodecContext->height;
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
	m_bReadFinished = false;
	m_bDecoderFinished = false;
	if (nullptr == m_ptrPCMBuffer)
	{
		m_ptrPCMBuffer = new Buffer();
		m_ptrPCMBuffer->initBuffer(1024 * 10);
	}

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
	m_ReadCV.notify_all();
	m_queueWaitDecodedCV.notify_all ();
	m_queueWaitConsumedCV.notify_all();
	if (m_ReadThread.joinable())
	{
		m_ReadThread.join();
	}
	if (m_DecoderThread.joinable())
	{
		m_DecoderThread.join();
	}
	if (m_ConsumeThread.joinable())
	{
		m_ConsumeThread.join();
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
	}
	if (swrContext)
	{
		swr_free(&swrContext);
	}
	if (videoCodecContext)
	{
		avcodec_free_context(&videoCodecContext);
	}
	if (audioCodecContext)
	{
		avcodec_free_context(&audioCodecContext);
	}
	if (formatContext)
	{
		avformat_close_input(&formatContext);
	}
}

void VideoDecoder::readFrameFromFile()
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
	m_bReadFinished = true;
}

void VideoDecoder::decoder()
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
			if (m_bReadFinished && m_queueNeedDecoderPacket.size() <= 0)
			{
				break;
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
				decoderVideo(packet);
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
	m_bDecoderFinished = true;
}

void VideoDecoder::decoderVideo(AVPacket* packet)
{
	AVFrame* frame = av_frame_alloc();
	if (avcodec_send_packet(videoCodecContext, packet) == 0)
	{
		int ret = avcodec_receive_frame(videoCodecContext, frame);
		while (ret == 0)
		{
			LOG_INFO("Video Decoder Begin Handle");
			AVFrame* yuvFrame = av_frame_alloc();
			double pts = frame->pts * av_q2d(formatContext->streams[videoStreamIndex]->time_base);
			av_image_alloc(yuvFrame->data, yuvFrame->linesize, m_stuVideoInfo.width, m_stuVideoInfo.height, m_stuVideoInfo.videoFormat, 1);
			sws_scale(swsContext, frame->data, frame->linesize, 0, videoCodecContext->height, yuvFrame->data, yuvFrame->linesize);
			std::shared_ptr<VideoCallbackInfo> videoInfo = std::make_shared<VideoCallbackInfo>();
			videoInfo->width = m_stuVideoInfo.width;
			videoInfo->height = m_stuVideoInfo.height;
			videoInfo->videoFormat = m_stuVideoInfo.videoFormat;
			videoInfo->m_dPts = pts;
			// 计算avframe中的数据量
			switch (m_stuVideoInfo.videoFormat)
			{
			case AV_PIX_FMT_YUV420P:
			{
				videoInfo->yuvData = new uint8_t[m_stuVideoInfo.width * m_stuVideoInfo.height * 3 / 2];
				videoInfo->dataSize = m_stuVideoInfo.width * m_stuVideoInfo.height * 3 / 2;
				memcpy(videoInfo->yuvData, yuvFrame->data[0], videoInfo->dataSize);
			}
			break;
			case AV_PIX_FMT_YUV422P:
			case AV_PIX_FMT_YUYV422:
			case AV_PIX_FMT_UYVY422:
			{
				videoInfo->yuvData = new uint8_t[m_stuVideoInfo.width * m_stuVideoInfo.height * 2];
				videoInfo->dataSize = m_stuVideoInfo.width * m_stuVideoInfo.height * 2;
				memcpy(videoInfo->yuvData, yuvFrame->data[0], videoInfo->dataSize);
			}
			break;
			default:
				break;
			}
			av_freep(yuvFrame->data);
			av_frame_free(&yuvFrame);
			if (nullptr == videoInfo->yuvData)
			{
				LOG_ERROR("videoInfo.yuvData is nullptr");
				ret = avcodec_receive_frame(videoCodecContext, frame);
				continue;
			}
			LOG_INFO("Decoder Wait Consume Lock");
			std::unique_lock<std::mutex> lck(m_afterDecoderInfoMutex);
			LOG_INFO("Decoder Get Consume Lock,Before Consume Video Queue Size:{}", m_queueVideoInfo.size());
			m_queueVideoInfo.push(videoInfo);
			lck.unlock();
			LOG_INFO("Decoder Release Consume Lock");
			//完成了一个视频解码，通知消费线程
			m_queueWaitConsumedCV.notify_one();
			ret = avcodec_receive_frame(videoCodecContext, frame);
		}
	}
	else
	{
		LOG_ERROR("video avcodec_send_packet error");
	}
	LOG_INFO("Decoder Video End");
	av_frame_free(&frame);
}

void VideoDecoder::decoderAudio(AVPacket* packet)
{
	AVFrame* frame = av_frame_alloc();
	int swr_size = 0;
	int resampled_linesize;
	int max_resampled_samples = 0;
	uint8_t** resampled_data = nullptr;
	if (avcodec_send_packet(audioCodecContext, packet) == 0)
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

			//std::fstream fsbefore("audio_before_resample", std::ios::app | std::ios::binary);
			//fsbefore.write((const char*)frame->data[0], audioCodecContext->channels*frame->nb_samples* av_get_bytes_per_sample(audioCodecContext->sample_fmt));
			//fsbefore.close();

			//std::fstream fs("audio.pcm", std::ios::app | std::ios::binary);
			////把重采样之后的数据保存本地
			//fs.write((const char*)resampled_data[0], pcmNumber);
			//fs.close();

			if (resampled_data)
			{
				m_ptrPCMBuffer->appendData(resampled_data[0], pcmNumber);
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

void VideoDecoder::consume()
{
	if (!m_bInitState)
	{
		return;
	}
	while (m_queueVideoInfo.size() < 15)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	auto needPaintTime = std::chrono::system_clock::now();
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
		LOG_INFO("Consumer Wait Consume Lock");
		std::unique_lock<std::mutex> lck(m_afterDecoderInfoMutex);
		LOG_INFO("Consumer Get Consume Lock,Before Size:{}", m_queueVideoInfo.size());
		//解码后的视频队列和音频队列都为空，就等待解码线程解码
		if (m_queueVideoInfo.size() <= 0 )
		{
			if (m_bDecoderFinished)
			{
				break;
			}
			printf("wait................\n");
			m_queueWaitConsumedCV.wait(lck, [this]
				{ return !m_bRunningState || m_queueVideoInfo.size() > 0; });
		}
		if (!m_bRunningState)
		{
			break;
		}
		std::shared_ptr<VideoCallbackInfo> videoInfo = nullptr;
		std::shared_ptr<AudioCallbackInfo> audioInfo = nullptr;
		if (m_queueVideoInfo.size() > 0)
		{
			videoInfo = m_queueVideoInfo.front();
			m_queueVideoInfo.pop();
		}
		LOG_INFO("Consumer Release Consume Lock");
		lck.unlock();
		//if (m_queueAudioInfo.size() > 0)
		//{
		//	audioInfo = m_queueAudioInfo.front();
		//	m_queueAudioInfo.pop();
		//}
		if (/*nullptr == audioInfo ||*/ nullptr == videoInfo)
		{
			continue;
		}
		//消费了一个以后，通知一下解码线程，可以继续工作
		m_queueWaitConsumedCV.notify_all();

		//计算需要休眠多少
		auto currentTime = std::chrono::system_clock::now();
		auto diff = std::chrono::duration_cast<std::chrono::microseconds>(needPaintTime - currentTime).count();
		if (diff > 0)
		{
			//std::this_thread::sleep_for(std::chrono::microseconds(diff));
			preciseSleep(std::chrono::duration_cast<std::chrono::microseconds>(needPaintTime - currentTime));
		}
		std::string t1 = getTime(currentTime);
		std::string t2 = getTime(needPaintTime);
		needPaintTime = needPaintTime + std::chrono::milliseconds(m_uiReadThreadSleepTime);
		std::string t = getTime(needPaintTime);
		LOG_INFO("Need Paint Time:{},Get Frame Time{},Next Paint Time:{}", t2, t1, t);
		if (m_previewCallback)
		{
			m_previewCallback(videoInfo, videoInfo->m_dPts);
		}
		if (m_audioPlayCallback)
		{
			int length = m_uiPerFrameSampleCnt * kOutputAudioChannels * av_get_bytes_per_sample((AVSampleFormat)kOutputAudioFormat);
			uint8_t* pcmData = new uint8_t[length]{ 0 };
			m_ptrPCMBuffer->getBuffer(pcmData, length);
			m_audioPlayCallback(pcmData, length);
			delete[]pcmData;
		}
		LOG_INFO("Consume End");
	}
	delete m_ptrPCMBuffer;
	m_ptrPCMBuffer = nullptr;
	if (swsContext)
	{
		sws_freeContext(swsContext);
	}
	if (swrContext)
	{
		swr_free(&swrContext);
	}
	if (videoCodecContext)
	{
		avcodec_free_context(&videoCodecContext);
	}
	if (audioCodecContext)
	{
		avcodec_free_context(&audioCodecContext);
	}
	if (formatContext)
	{
		avformat_close_input(&formatContext);
	}
}

/*
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
					//qDebug() << "video pts" << pts;
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

				//qDebug() << "video start time" << m_iStartTime << ",play time" << current_time << "seek time" << m_iTotalVideoSeekTime << ",delay:" << delay << ",pts:" << pts;

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
				//std::fstream fs("audio.pcm", std::ios::app | std::ios::binary);
				////把重采样之前的数据保存本地
				//fs.write((const char *)frame->data[0], frame->linesize[0]);
				//fs.close();

				int data_size = av_samples_get_buffer_size(nullptr,
														   audioCodecContext->ch_layout.nb_channels,
														   frame->nb_samples,
														   audioCodecContext->sample_fmt, 1);
				int32_t out_buffer_size = av_samples_get_buffer_size(nullptr, m_stuAudioInfo.audioChannels, frame->nb_samples, m_stuAudioInfo.audioFormat, 1);

				//// 分配输出缓冲区的空间
				uint8_t *out_buff = (unsigned char *)av_malloc(out_buffer_size);
				swr_size = swr_convert(swrContext,										  // 音频采样器的实例
									   &out_buff, frame->nb_samples,					  // 输出的数据内容和数据大小
									   (const uint8_t **)frame->data, frame->nb_samples); // 输入的数据内容和数据大小

				double pts = frame->pts * av_q2d(formatContext->streams[audioStreamIndex]->time_base);
				// 记录下当前播放的帧的时间，用于计算快进时的增量

				if (pts > 0)
				{
					//qDebug() << "audio pts" << pts;
					if (m_bFirstAudioPacketAfterSeek)
					{
						m_bFirstAudioPacketAfterSeek = false;
					}
					m_uiAudioCurrentTime = pts * kmicroSecondsPerSecond;
				}

				int64_t current_time = av_gettime() - m_iStartTime + m_iTotalAudioSeekTime;
				int64_t delay = static_cast<int64_t>(pts * kmicroSecondsPerSecond) - current_time;

				//qDebug() << "audio start time" << m_iStartTime << ",play time" << current_time << "seek time" << m_iTotalAudioSeekTime << ",delay:" << delay << ",pts:" << pts;

				if (delay > 0)
				{
					std::this_thread::sleep_for(std::chrono::microseconds(delay));
				}

				if (m_audioPlayCallback && !m_bSeekState)
				{
					m_audioPlayCallback(out_buff, frame->nb_samples);
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
*/

void VideoDecoder::initVideoCallBack(PreviewCallback preCallback, VideoOutputCallback videoOutputCallback)
{
	m_previewCallback = preCallback;
	m_videoOutputCallback = videoOutputCallback;
}

void VideoDecoder::pauseDecoder()
{
	std::unique_lock<std::mutex> lck(m_PauseMutex);
	m_bPauseState = true;
}

void VideoDecoder::resumeDecoder()
{
	std::unique_lock<std::mutex> lck(m_PauseMutex);
	m_bPauseState = false;
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

	m_ReadThread = std::thread(&VideoDecoder::readFrameFromFile, this);

	m_DecoderThread = std::thread(&VideoDecoder::decoder, this);

	m_ConsumeThread = std::thread(&VideoDecoder::consume, this);
}